/* @flow */

'use strict'

import {IndexedIterable} from 'immutable'

import crypto from 'crypto'
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

type FileFreshness = 'fresh' | 'updating' | 'stale';

/**
 * Describes a file in the system that should be dealt with.
 */
type File = {
  /**
   * Does it need to be refreshed to complete the update? Stale files might not
   * even exist in the filesystem.
   */
  freshness: FileFreshness,
  /**
   * A set of files that depend on this one.
   */
  successors: immutable.Set<string>,
  /**
   * A set of files this one depends on.
   */
  predecessors: immutable.Set<string>,
  /**
   * The nature of the file. IT'll use different stategies to update the file
   * depending on its type.
   */
  type: 'header' | 'program' | 'object' | 'source',
}

type State = {
  files: immutable.Map<string, File>,
}

type Event = {
  type: 'start',
  sourceFilePaths: IndexedIterable<string>,
  programFilePath: string,
} | {
  type: 'fileUpdated',
  filePath: string,
}

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

function newFile(freshness, type): File {
  return {
    freshness,
    successors: immutable.Set(),
    predecessors: immutable.Set(),
    type,
  }
}

function addFileEdge(
  files: immutable.Map<string, File>,
  fromPath: string,
  toPath: string
): immutable.Map<string, File> {
  const fromFile = files.get(fromPath)
  const toFile = files.get(toPath)
  return files.set(fromPath, {
    freshness: fromFile.freshness,
    successors: fromFile.successors.add(toPath),
    predecessors: fromFile.predecessors,
    type: fromFile.type,
  }).set(toPath, {
    freshness: toFile.freshness,
    successors: toFile.successors,
    predecessors: toFile.predecessors.add(fromPath),
    type: toFile.type,
  })
}

function setFileFreshness(file: File, freshness: FileFreshness): File {
  return {
    freshness,
    successors: file.successors,
    predecessors: file.predecessors,
    type: file.type,
  }
}

class UpdAgent {

  _state: ?State;
  _opts: {verbose: boolean};

  constructor(opts: Object) {
    this._opts = opts
  }

  _spawn(cmd: string, args: Array<string>) {
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

  _updateObject(file: File, filePath: string) {
    const depFilePath = filePath + '.d'
    return this._spawn(
      'clang++',
      [
        '-c', '-o', filePath,
        '-Wall', '-std=c++14', '-fcolor-diagnostics',
        '-MMD', '-MF', depFilePath,
      ].concat(file.predecessors.toArray())
    ).then(() => new Promise((resolve, reject) => {
      fs.readFile(depFilePath, 'utf8', (error, content) => {
        if (error) {
          return reject(error)
        }
        // Ain't got no time for a true parser.
        const chunks = content.split(/(?:\n| )/)
          .filter(chunk => chunk.endsWith('.h'))
          .map(filePath => path.normalize(filePath))
        resolve()
      })
    }))
  }

  _updateProgram(file: File, filePath: string): Promise {
    return this._spawn(
      'clang++',
      [
        '-o', filePath, '-framework', 'OpenGL',
        '-Wall', '-std=c++14', '-lglew', '-lglfw3',
        '-fcolor-diagnostics',
      ].concat(file.predecessors.toArray())
    )
  }

  _updateFile(file: File, filePath: string): Promise {
    switch (file.type) {
      case 'object':
        return this._updateObject(file, filePath)
      case 'program':
        return this._updateProgram(file, filePath)
    }
    return Promise.reject(new Error(`don\'t know how to build '${filePath}'`))
  }

  _startUpdateFile(file: File, filePath: string): File {
    this._updateFile(file, filePath).then(() => {
      return this.update({type: 'fileUpdated', filePath})
    }).catch(error => process.nextTick(() => { throw error }))
    return setFileFreshness(file, 'updating')
  }

  _reduceStart(
    sourceFilePaths: IndexedIterable<string>,
    programFilePath: string
  ): State {
    mkdirp.sync(UPD_CACHE_PATH)
    let files = new immutable.Map([
      [programFilePath, newFile('stale', 'program')]
    ])
    sourceFilePaths.forEach(sourceFilePath => {
      const objectFilePath = path.join(UPD_CACHE_PATH, sha1(sourceFilePath))
      if (files.has(objectFilePath)) {
        throw new Error('object file hash collision')
      }
      files = files.set(sourceFilePath, newFile('fresh', 'source'))
        .set(objectFilePath, newFile('stale', 'object'))
      files = addFileEdge(files, sourceFilePath, objectFilePath)
      files = addFileEdge(files, objectFilePath, programFilePath)
    })
    files = files.map((file, filePath) => {
      if (
        file.freshness === 'stale' &&
        file.predecessors.every(predecessorPath => (
          files.get(predecessorPath).freshness === 'fresh'
        ))
      ) {
        return this._startUpdateFile(file, filePath)
      }
      return file
    })
    return {
      files,
    }
  }

  _reduceFileUpdated(state: State, filePath: string): State {
    let file = state.files.get(filePath)
    if (file.freshness !== 'updating') {
      return state
    }
    file = setFileFreshness(file, 'fresh')
    let files = state.files.set(filePath, file)
    file.successors.forEach(successorPath => {
      const successor = files.get(successorPath)
      if (successor.predecessors.every(predecessorPath => (
        files.get(predecessorPath).freshness === 'fresh'
      ))) {
        files = files.set(
          successorPath,
          this._startUpdateFile(successor, successorPath)
        )
      }
    })
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
      case 'fileUpdated':
        return this._reduceFileUpdated(state, event.filePath)
    }
  }

  _writeStatus(text: string) {
    if (process.stdout.isTTY) {
      ;(readline: any).clearLine(process.stdout, 0)
      ;(readline: any).cursorTo(process.stdout, 0)
    }
    process.stdout.write(text, 'utf8')
    if (!process.stdout.isTTY && text.length > 0) {
      process.stdout.write('\n', 'utf8')
    }
  }

  _refresh(state: ?State) {
    if (state == null) {
      return
    }
    const freshCount = (
      state.files.toSeq().filter(file => file.freshness === 'fresh').count()
    )
    if (freshCount < state.files.size) {
      this._writeStatus(`updating [${freshCount}/${state.files.size}]`)
    } else {
      if (!state.files.some(file => file.freshness === 'updating')) {
        this._writeStatus('')
      } else {
        this._writeStatus('some files cannot be built')
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
  const opts = nopt({'verbose': Boolean})
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
