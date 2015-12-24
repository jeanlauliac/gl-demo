/* @flow */

'use strict'

import Digraph from './Digraph'
import FileStatus from './FileStatus'
import crypto from 'crypto'
import chokidar from 'chokidar'
import fs from 'fs'
import glob from 'glob'
import immutable from 'immutable'
import mkdirp from 'mkdirp'
import nopt from 'nopt'
import path from 'path'
import readline from 'readline'
import {spawn} from 'child_process'

function sha1(data) {
  var shasum = crypto.createHash('sha1')
  shasum.update(data)
  return shasum.digest('hex')
}

const UPD_CACHE_PATH = '.upd_cache'

type FileRelation = 'source' | 'dependency';
type FileGraph = Digraph<string, FileStatus, FileRelation>

type State = {
  files: FileGraph,
}

type Event = {
  type: 'start',
  sourceFilePaths: immutable._Iterable_Indexed<string>,
  programFilePath: string,
} | {
  depPaths: immutable._Iterable_Indexed<string>,
  filePath: string,
  type: 'fileUpdated',
} | {
  filePath: string,
  type: 'fileChanged',
}

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

class UpdAgent {

  _state: ?State;
  _statusText: string;
  _opts: {
    once: boolean,
    verbose: boolean,
  };

  constructor(opts: Object) {
    this._opts = opts
    this._statusText = ''
  }

  _spawn(cmd: string, args: Array<string>): Promise<void> {
    if (this._opts.verbose) {
      const argsStr = args.map(escapeShellArg).join(' ')
      console.log(`${cmd} ${argsStr}`)
    }
    return new Promise((resolve, reject) => {
      const childProcess = spawn(cmd, args)
      childProcess.on('exit', code => {
        childProcess.stdout.pipe(process.stdout)
        childProcess.stderr.pipe(process.stderr)
        if (code === 0) {
          return resolve()
        }
        reject(new Error('spawn program failed'))
      })
    })
  }

  _updateObject(
    files: FileGraph,
    file: FileStatus,
    filePath: string
  ): Promise<immutable._Iterable_Indexed<string>> {
    const depFilePath = filePath + '.d'
    return this._spawn(
      'clang++',
      [
        '-c', '-o', filePath,
        '-Wall', '-std=c++14', '-fcolor-diagnostics',
        '-MMD', '-MF', depFilePath,
      ].concat(files.preceding(filePath).filter(link => (
        link.value === 'source'
      )).keySeq().toArray())
    ).then(() => new Promise((resolve, reject) => {
      fs.readFile(depFilePath, 'utf8', (error, content) => {
        if (error) {
          return reject(error)
        }
        // Ain't got no time for a true parser.
        const depPaths = new immutable.List(content.split(/(?:\n| )/)
          .filter(chunk => chunk.endsWith('.h'))
          .map(filePath => path.normalize(filePath)))
        resolve(depPaths)
      })
    }))
  }

  _updateProgram(
    files: FileGraph,
    file: FileStatus,
    filePath: string
  ): Promise<immutable._Iterable_Indexed<string>> {
    return this._spawn(
      'clang++',
      [
        '-o', filePath, '-framework', 'OpenGL',
        '-Wall', '-std=c++14', '-lglew', '-lglfw3',
        '-fcolor-diagnostics',
      ].concat(files.preceding(filePath).keySeq().toArray())
    ).then(() => new immutable.List())
  }

  _updateFile(
    files: FileGraph,
    file: FileStatus,
    filePath: string
  ): Promise<immutable._Iterable_Indexed<string>> {
    switch (file.type) {
      case 'object':
        return this._updateObject(files, file, filePath)
      case 'program':
        return this._updateProgram(files, file, filePath)
    }
    return Promise.reject(new Error(`don\'t know how to build '${filePath}'`))
  }

  _startUpdateFile(
    files: FileGraph,
    file: FileStatus,
    filePath: string
  ): FileStatus {
    this._updateFile(files, file, filePath).then((depPaths) => {
      return this.update({type: 'fileUpdated', filePath, depPaths})
    }).catch(error => process.nextTick(() => { throw error }))
    return file.setFreshness('updating')
  }

  _startWatching() {
    if (this._opts.once) {
      return
    }
    chokidar.watch(['ds', 'glfwpp', 'glpp', 'main.cpp'])
      .on('change', filePath => {
        this.update({type: 'fileChanged', filePath})
      })
  }

  _reduceStart(
    sourceFilePaths: immutable._Iterable_Indexed<string>,
    programFilePath: string
  ): State {
    mkdirp.sync(UPD_CACHE_PATH)
    this._startWatching()
    let files = Digraph.empty()
      .set(programFilePath, FileStatus.create('stale', 'program'))
    sourceFilePaths.forEach(sourceFilePath => {
      const objectFilePath = path.join(UPD_CACHE_PATH, sha1(sourceFilePath))
      if (files.has(objectFilePath)) {
        throw new Error('object file hash collision')
      }
      files = files.set(sourceFilePath, FileStatus.create('fresh', 'none'))
        .set(objectFilePath, FileStatus.create('stale', 'object'))
      files = files.link(sourceFilePath, objectFilePath, 'source')
      files = files.link(objectFilePath, programFilePath, 'source')
    })
    files = files.map((file, filePath) => {
      if (
        file.freshness === 'stale' &&
        files.preceding(filePath).every(predecessor => (
          predecessor.origin.freshness === 'fresh'
        ))
      ) {
        return this._startUpdateFile(files, file, filePath)
      }
      return file
    })
    return {files}
  }

  _reduceFileChanged(
    state: State,
    filePath: string
  ): State {
    const file = state.files.get(filePath)
    if (!state.files.preceding(filePath).isEmpty()) {
      return state
    }
    let files = state.files
    let successors = new immutable.List(
      state.files.following(filePath).entrySeq()
    )
    while (successors.size > 0) {
      let [successorPath, link] = successors.first()
      successors = successors.shift()
      if (link.target.freshness === 'stale') {
        continue
      }
      const successor = link.target.setFreshness('stale')
      files = files.set(successorPath, successor)
      successors = successors.concat(files.following(successorPath).entrySeq())
    }
    files = files.map((file, filePath) => {
      if (
        file.freshness === 'stale' &&
        files.preceding(filePath).every(predecessor => (
          predecessor.origin.freshness === 'fresh'
        ))
      ) {
        return this._startUpdateFile(files, file, filePath)
      }
      return file
    })
    return {files}
  }

  _reduceFileUpdated(
    state: State,
    filePath: string,
    depPaths: immutable._Iterable_Indexed<string>
  ): State {
    let file = state.files.get(filePath)
    if (file == null || file.freshness !== 'updating') {
      return state
    }
    file = file.setFreshness('fresh')
    let files = state.files.set(filePath, file)
    depPaths.forEach(depPath => {
      if (!files.has(depPath)) {
        files = files.set(depPath, FileStatus.create('fresh', 'none'))
      }
      files = files.link(depPath, filePath, 'dependency');
    })
    files.following(filePath).forEach((successor, successorPath) => {
      if (files.preceding(successorPath).every(predecessor => (
        predecessor.origin.freshness === 'fresh'
      ))) {
        files = files.set(
          successorPath,
          this._startUpdateFile(files, successor.target, successorPath)
        )
      }
    })
    if (!this._opts.once) {
      // TODO: watch predecessors if they're not already
      // watch source as soon as we know them, so as to cancel the update if
      // file change *during* updating (ideally, kill the compile process)
    }
    return {files}
  }

  _reduce(state: ?State, event: Event): ?State {
    switch (event.type) {
      case 'start':
        return this._reduceStart(event.sourceFilePaths, event.programFilePath)
    }
    if (state == null) {
      return state
    }
    switch (event.type) {
      case 'fileChanged':
        return this._reduceFileChanged(state, event.filePath)
      case 'fileUpdated':
        return this._reduceFileUpdated(state, event.filePath, event.depPaths)
    }
  }

  _log(text: string) {
    const statusText = this._statusText
    if (process.stdout.isTTY) {
      this._updateStatus('');
    }
    process.stdout.write(text + '\n', 'utf8')
    if (process.stdout.isTTY) {
      this._updateStatus(statusText);
    }
  }

  _updateStatus(text: string) {
    if (process.stdout.isTTY && this._statusText.length > 0) {
      ;(readline: any).clearLine(process.stdout, 0)
      ;(readline: any).cursorTo(process.stdout, 0)
    }
    if (text.length > 0) {
      process.stdout.write(text, 'utf8')
    }
    if (!process.stdout.isTTY && text.length > 0) {
      process.stdout.write('\n', 'utf8')
    }
    this._statusText = text
  }

  _flushStatus() {
    if (process.stdout.isTTY) {
      process.stdout.write('\n', 'utf8')
    }
    this._statusText = ''
  }

  _refresh(state: ?State) {
    if (state == null) {
      return
    }
    const freshCount = (
      state.files.toSeq().filter(file => file.freshness === 'fresh').count()
    )
    const prc = Math.round((freshCount / state.files.order) * 100)
    const isDone = freshCount === state.files.order
    const doneText = isDone ? ', done.' : ''
    this._updateStatus(
      `Updating, ${prc}% (${freshCount}/${state.files.order})${doneText}`
    )
    if (isDone) {
      this._flushStatus();
      if (!this._opts.once) {
        this._updateStatus('Watching...');
      }
      if (state.files.some(file => file.freshness === 'stale')) {
        this._log('some files cannot be built')
        process.exit(1)
      }
    }
  }

  update(event: Event) {
    const nextState = this._reduce(this._state, event)
    if (this._state === nextState) {
      return
    }
    this._state = nextState
    this._refresh(nextState)
  }

}

;(() => {
  const opts = nopt({verbose: Boolean, once: Boolean})
  const updAgent = new UpdAgent(opts);
  updAgent.update({
    programFilePath: 'gl-demo',
    sourceFilePaths: (immutable: any).Iterable.Indexed(['main.cpp']
      .concat(glob.sync('glfwpp/*.cpp'))
      .concat(glob.sync('glpp/*.cpp'))
      .concat(glob.sync('ds/*.cpp'))
    ),
    type: 'start',
  })
})()
