/* @flow */
import {spawn} from 'child_process'
import glob from 'glob'

;(() => {
  const clang = spawn(
    'clang++',
    [
      '-o', 'gl-demo', '-framework', 'OpenGL',
      '-Wall', '-std=c++14', '-lglew', '-lglfw3',
      'main.cpp'
    ].concat(glob.sync('glfwpp/*.cpp'))
     .concat(glob.sync('glpp/*.cpp'))
     .concat(glob.sync('ds/*.cpp'))
  )
  clang.stdout.pipe(process.stdout)
  clang.stderr.pipe(process.stderr)
})()
