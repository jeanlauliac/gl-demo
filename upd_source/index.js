/* @flow */

'use strict'

import crypto from 'crypto'
import glob from 'glob'
import immutable from 'immutable'
import mkdirp from 'mkdirp'
import nopt from 'nopt'
import path from 'path'
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
}

type Event = {
  type: 'start',
  sourceFilePaths: immutable.Iterable<string>,
  programFilePath: string,
} | {
  type: 'fileUpdated',
  filePath: string,
}

function verboseSpawn(cmd, args) {
  const argsStr = args.map(escapeShellArg).join(' ')
  console.log(`${cmd} ${argsStr}`)
  return spawn(cmd, args)
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
    this._spawn = opts.verbose ? verboseSpawn : spawn
  }

  _startUpdateObject(file: File, filePath: string) {
    const clang = this._spawn(
      'clang++',
      [
        '-c', '-o', filePath,
        '-Wall', '-std=c++14', '-fcolor-diagnostics',
        '-MMD', '-MF', filePath + '.d',
      ].concat(file.predecessors.toArray())
    ).on('exit', (code) => {
      clang.stdout.pipe(process.stdout)
      clang.stderr.pipe(process.stderr)
      if (code === 0) {
        return this.update({type: 'fileUpdated', filePath})
      }
      throw new Error('failed to build file')
    })
  }

  _startUpdateProgram(file: File, filePath: string) {
    const clang = this._spawn(
      'clang++',
      [
        '-o', filePath, '-framework', 'OpenGL',
        '-Wall', '-std=c++14', '-lglew', '-lglfw3',
        '-fcolor-diagnostics',
      ].concat(file.predecessors.toArray())
    ).on('exit', (code) => {
      clang.stdout.pipe(process.stdout)
      clang.stderr.pipe(process.stderr)
      if (code === 0) {
        return this.update({type: 'fileUpdated', filePath})
      }
      throw new Error('failed to build file')
    })
  }

  _startUpdateFile(file: File, filePath: string) {
    switch (file.type) {
      case 'object':
        return this._startUpdateObject(file, filePath)
      case 'program':
        return this._startUpdateProgram(file, filePath)
    }
    throw new Error(`don\'t know how to build '${filePath}'`)
  }

  _reduceStart(
    sourceFilePaths: immutable.Iterable<string>,
    programFilePath: string
  ): State {
    mkdirp.sync(UPD_CACHE_PATH)
    let files = immutable.Map({[programFilePath]: newFile('stale', 'program')})
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
    return {files}
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
    return {files}
  }

  _reduce(state: ?State, event: Event): State {
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

  update(event: Event) {
    const nextState = this._reduce(this._state, event)
    if (nextState) {
      this._state = nextState
    }
  }

}

;(() => {
  const opts = nopt({'verbose': Boolean})
  const updAgent = new UpdAgent(opts);
  updAgent.update({
    programFilePath: 'gl-demo',
    sourceFilePaths: immutable.Iterable(['main.cpp']
      .concat(glob.sync('glfwpp/*.cpp'))
      .concat(glob.sync('glpp/*.cpp'))
      .concat(glob.sync('ds/*.cpp'))
    ),
    type: 'start',
  })
})()
