/* @noflow */

'use strict';

import * as FileStatus from './FileStatus';
import Digraph from './Digraph';
import {spawn} from 'child_process';
import chokidar from 'chokidar';
import crypto from 'crypto';
import fs from 'fs';
import glob from 'glob';
import immutable from 'immutable';
import mkdirp from 'mkdirp';
import nopt from 'nopt';
import path from 'path';
import readline from 'readline';
import {Writable} from 'stream';

export type UpdateResult = 'failure' | 'success';

function sha1(data) {
  var shasum = crypto.createHash('sha1')
  shasum.update(data)
  return shasum.digest('hex')
}

const UPD_CACHE_PATH = '.upd_cache'

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

class BufferStream extends Writable {
  constructor() {
    super()
    this._bufs = []
  }

  _write(chunk, encoding, callback) {
    this._bufs.push(chunk)
    callback()
  }

  toBuffer() {
    return Buffer.concat(this._bufs)
  }
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

  _spawn(cmd: string, args: Array<string>): Promise<UpdateResult> {
    if (this._opts.verbose) {
      const argsStr = args.map(escapeShellArg).join(' ')
      this._log(`${cmd} ${argsStr}`)
    }
    return new Promise((resolve, reject) => {
      const childProcess = spawn(cmd, args)
      const outBufferStream = new BufferStream()
      childProcess.stdout.pipe(outBufferStream)
      const errBufferStream = new BufferStream()
      childProcess.stderr.pipe(errBufferStream)
      childProcess.on('exit', code => {
        const outBuffer = outBufferStream.toBuffer()
        const errBuffer = errBufferStream.toBuffer()
        const updateResult = (code === 0) ? 'success' : 'failure'
        if (outBuffer.length > 0 || errBuffer.length > 0) {
          this._flushStatus();
          process.stdout.write(outBuffer)
          return void process.stderr.write(errBuffer, null, () => {
            resolve(updateResult)
          })
        }
        resolve(updateResult)
      })
    })
  }

  _updateObject(
    files: FileGraph,
    file: FileStatus.Status,
    filePath: string
  ): Promise<[UpdateResult, immutable._Iterable_Indexed<string>]> {
    const depFilePath = filePath + '.d'
    return this._spawn(
      'clang++',
      [
        '-c', '-o', filePath,
        '-Wall', '-std=c++14', '-fcolor-diagnostics',
        '-MMD', '-MF', depFilePath,
        '-I', '/usr/local/include',
      ].concat(files.preceding(filePath).filter(link => (
        link.value === 'source'
      )).keySeq().toArray())
    ).then(updateResult => new Promise((resolve, reject) => {
      if (updateResult !== 'success') {
        return void resolve([updateResult, immutable.Iterable.Indexed()])
      }
      fs.readFile(depFilePath, 'utf8', (error, content) => {
        if (error) {
          return reject(error)
        }
        // Ain't got no time for a true parser.
        const depPaths = new immutable.List(content.split(/(?:\n| )/)
          .filter(chunk => chunk.endsWith('.h'))
          .map(filePath => path.normalize(filePath)))
        resolve([updateResult, depPaths])
      })
    }))
  }

  _updateProgram(
    files: FileGraph,
    file: FileStatus.Status,
    filePath: string
  ): Promise<[UpdateResult, immutable._Iterable_Indexed<string>]> {
    return this._spawn(
      'clang++',
      [
        '-o', filePath, '-framework', 'OpenGL',
        '-Wall', '-std=c++14', '-lglew', '-lglfw3',
        '-fcolor-diagnostics', '-L', '/usr/local/lib',
      ].concat(files.preceding(filePath).keySeq().toArray())
    ).then(updateResult => [updateResult, immutable.Iterable.Indexed()])
  }

  _updateFile(
    files: FileGraph,
    file: FileStatus.Status,
    filePath: string
  ): Promise<[UpdateResult, immutable._Iterable_Indexed<string>]> {
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
    file: FileStatus.Status,
    filePath: string
  ): FileStatus.Status {
    this._updateFile(files, file, filePath).then(([updateResult, depPaths]) => {
      if (updateResult === 'success') {
        return void this.update({type: 'fileUpdated', filePath, depPaths})
      }
      return void this.update({type: 'fileUpdateFailed', filePath})
    }).catch(error => process.nextTick(() => { throw error }))
    return FileStatus.set(file, 'updating')
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
      const successor = FileStatus.set(link.target, 'stale')
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
    file = FileStatus.set(file, 'fresh')
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
    return {files}
  }

  _reduceFileUpdateFailed(state: State, filePath: string): State {
    let file = state.files.get(filePath)
    if (file == null || file.freshness !== 'updating') {
      return state
    }
    file = FileStatus.set(file, 'stale')
    return {files: state.files.set(filePath, file)}
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
      case 'fileUpdateFailed':
        return this._reduceFileUpdateFailed(state, event.filePath)
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
    const filesToBuild = (
      state.files.toSeq()
      .filter(file => file.type !== 'none')
      .toList()
    )
    const freshCount = (
      filesToBuild
      .filter(file => file.freshness === 'fresh')
      .count()
    )
    const prc = Math.round((freshCount / filesToBuild.size) * 100)
    const isAllFresh = freshCount === filesToBuild.size
    const isUpdating = filesToBuild.some(file => file.freshness === 'updating')
    const finalText = isAllFresh ? ', done.' : (!isUpdating ? ', failed.' : '')
    this._updateStatus(
      `Updating, ${prc}% (${freshCount}/${filesToBuild.size})${finalText}`
    )
    if (isAllFresh) {
      this._flushStatus();
      if (!this._opts.once) {
        this._updateStatus('Watching...')
      }
    } else if (!isUpdating) {
      this._flushStatus();
      if (this._opts.once) {
        process.exit(1)
      } else {
        this._updateStatus('Watching...')
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
  if (process.getuid() <= 0) {
    process.stderr.write('Cowardly refusing to execute as root.\n');
    return process.exit(124);
  }
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
