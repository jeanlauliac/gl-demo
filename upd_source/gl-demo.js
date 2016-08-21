/* @flow */

'use strict';

import type {Process} from './process';

import * as adjacency_list from './adjacency_list';
import chain from './chain';
import cli from './cli';
import * as process from './process';
import glob from 'glob';
import * as immutable from 'immutable';
import path from 'path';

/**
 * Return the same file directory and name stripped of its extension.
 */
function pathWithoutExt(filePath: string): string {
  const ext = path.extname(filePath);
  return filePath.substr(0, filePath.length - ext.length);
}

/**
 * Promise version of fs.readFile.
 */
// function readFilePromise(filePath: string, encoding: string) {
//   return new Promise((resolve, reject) => {
//     fs.readFile(filePath, encoding, (error, content) => {
//       if (error) {
//         return void reject(error);
//       }
//       resolve(content);
//     });
//   });
// }

/**
 * Return a descriptor of the process that compiles the specified C++14 source
 * files into the specified object file.
 */
function compile(
  filePath: string,
  sourcePaths: immutable.Set<string>,
): Process {
  const depFilePath = pathWithoutExt(filePath) + '.d';
  return process.create('clang++', immutable.List([
    '-c', '-o', filePath, '-Wall', '-std=c++14', '-fcolor-diagnostics',
    '-MMD', '-MF', depFilePath, '-I', '/usr/local/include',
  ]).concat(sourcePaths.toArray()));
  // Implement this part in a pure functional way as well.
  // .then(result => {
  //   if (result !== 'built') {
  //     return Promise.resolve({result, dynamicDependencies: immutable.List()});
  //   }
  //   return readFilePromise(depFilePath, 'utf8').then(content => {
  //     // Ain't got no time for a true parser.
  //     const dynamicDependencies = immutable.List(content.split(/(?:\n| )/)
  //       .filter(chunk => chunk.endsWith('.h'))
  //       .map(filePath => path.normalize(filePath)));
  //     return {result, dynamicDependencies};
  //   });
  // });
}

/**
 * Return a descriptor of the process that links the specified binary object
 * files into the specified executable file.
 */
function link(
  filePath: string,
  sourcePaths: immutable.Set<string>,
): Process {
  return process.create('clang++', immutable.List([
    '-o', filePath, '-framework', 'OpenGL', '-Wall', '-std=c++14', '-lglew',
    '-lglfw3', '-fcolor-diagnostics', '-L', '/usr/local/lib',
  ]).concat(sourcePaths.toArray()));
}

/**
 * Entry point: build the lists of all the files we need to build.
 */
cli(cliOpts => {
  const sourceFiles = immutable.List(['main.cpp'])
    .concat(glob.sync('glfwpp/*.cpp'))
    .concat(glob.sync('glpp/*.cpp'))
    .concat(glob.sync('ds/*.cpp'));
  const sourceObjectPairs = sourceFiles.map(sourceFilePath => {
    const barePath = pathWithoutExt(sourceFilePath);
    const objectFilePath = path.join('.upd_cache', barePath + '.o');
    return [sourceFilePath, objectFilePath];
  });
  const fileAdjacencyList = (
    sourceObjectPairs.reduce((fileAdj, [sourcePath, objectPath]) => {
      return chain([
        fileAdj => adjacency_list.add(fileAdj, sourcePath, objectPath),
        fileAdj => adjacency_list.add(fileAdj, objectPath, 'gl-demo'),
      ], fileAdj);
    }, adjacency_list.empty())
  );
  const fileBuilders = sourceObjectPairs.reduce((builders, pair) => {
    return builders.set(pair[1], compile);
  }, immutable.Map()).set('gl-demo', link);
  return {cliOpts, fileAdjacencyList, fileBuilders};
});
