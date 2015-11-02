/* @flow */

'use strict'

import {IndexedIterable} from 'immutable'

import crypto from 'crypto'
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

/**
 * Describes a file in the system that should be dealt with.
 */
type File = {
  /**
   * Does it need to be refreshed to complete the update? Stale files might not
   * even exist in the filesystem.
   */
  freshness: 'fresh' | 'stale',
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
  jobCount: number,
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
    return this._spawn(
      'clang++',
      [
        '-c', '-o', filePath,
        '-Wall', '-std=c++14', '-fcolor-diagnostics',
        '-MMD', '-MF', filePath + '.d',
      ].concat(file.predecessors.toArray())
    )
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
    throw new Error(`don\'t know how to build '${filePath}'`)
  }

  _startUpdateFile(file: File, filePath: string) {
    this._updateFile(file, filePath).then(() => {
      return this.update({type: 'fileUpdated', filePath})
    }, error => process.nextTick(() => { throw error }))
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
    files.forEach((file, filePath) => {
      if (
        file.freshness === 'stale' &&
        file.predecessors.every(predecessorPath => (
          files.get(predecessorPath).freshness === 'fresh'
        ))
      ) {
        this._startUpdateFile(file, filePath)
      }
    })
    return {
      files,
      jobCount: files.toSeq().filter(file => file.freshness === 'stale').count()
    }
  }

  _reduceFileUpdated(state: State, filePath: string): State {
    let file = state.files.get(filePath)
    file = {
      freshness: 'fresh',
      successors: file.successors,
      predecessors: file.predecessors,
      type: file.type,
    }
    const files = state.files.set(filePath, file)
    file.successors.forEach(successorPath => {
      const successor = files.get(successorPath)
      if (successor.predecessors.every(predecessorPath => (
        files.get(predecessorPath).freshness === 'fresh'
      ))) {
        this._startUpdateFile(successor, successorPath)
      }
    })
    return {files, jobCount: state.jobCount - 1}
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

  _refresh(state: State) {
    if (process.stdout.isTTY) {
      ;(readline: any).clearLine(process.stdout, 0)
      ;(readline: any).cursorTo(process.stdout, 0)
    }
    if (state.jobCount > 0) {
      const complete = state.files.size - state.jobCount
      process.stdout.write(
        `updating [${complete}/${state.files.count()}]`,
        'utf8'
      )
    }
    if (!process.stdout.isTTY) {
      process.stdout.write('\n', 'utf8')
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
