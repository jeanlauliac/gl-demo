/* @flow */
import crypto from 'crypto'
import glob from 'glob'
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

;(() => {
  const opts = nopt({'verbose': Boolean})
  const vspawn = opts.verbose ? verboseSpawn : spawn
  mkdirp.sync(UPD_CACHE_PATH)
  const sourceFilePaths = ['main.cpp']
    .concat(glob.sync('glfwpp/*.cpp'))
    .concat(glob.sync('glpp/*.cpp'))
    .concat(glob.sync('ds/*.cpp'));
  const hashes = new Set()
  const objects = sourceFilePaths.map(sourceFilePath => {
    const hash = sha1(sourceFilePath)
    if (hashes.has(hash)) {
      throw new Error('object file hash collision')
    }
    hashes.add(hash)
    return {
      sourceFilePath,
      objectFilePath: path.join(UPD_CACHE_PATH, hash + '.o'),
      depFilePath: path.join(UPD_CACHE_PATH, hash + '.d'),
    }
  })
  Promise.all(objects.map(object => {
    return new Promise((resolve, reject) => {
      const clang = vspawn(
        'clang++',
        [
          '-c', object.sourceFilePath, '-o', object.objectFilePath,
          '-Wall', '-std=c++14', '-fcolor-diagnostics',
          '-MMD', '-MF', object.depFilePath,
        ]
      ).on('exit', (code) => {
        clang.stdout.pipe(process.stdout)
        clang.stderr.pipe(process.stderr)
        if (code === 0) {
          resolve()
        }
        reject(code)
      })
    })
  })).then(() => {
    const clang = vspawn(
      'clang++',
      [
        '-o', 'gl-demo', '-framework', 'OpenGL',
        '-Wall', '-std=c++14', '-lglew', '-lglfw3',
        '-fcolor-diagnostics',
      ].concat(objects.map(object => object.objectFilePath))
    )
    clang.stdout.pipe(process.stdout)
    clang.stderr.pipe(process.stderr)
  }).catch(error => { throw error })
})()

function verboseSpawn(cmd, args) {
  const argsStr = args.map(escapeShellArg).join(' ')
  console.log(`${cmd} ${argsStr}`)
  return spawn(cmd, args)
}

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}
