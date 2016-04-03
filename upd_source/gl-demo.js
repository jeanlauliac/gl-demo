/* @flow */

'use strict';

import * as adjacencyList from './adjacency-list';
import chain from './chain';
import cli from './cli';
import glob from 'glob';
import immutable from 'immutable';
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
function readFilePromise(filePath: string, encoding: string) {
  return new Promise((resolve, reject) => {
    fs.readFile(filePath, encoding, (error, content) => {
      if (error) {
        return void reject(error);
      }
      resolve(content);
    });
  });
}

/**
 * Compile the specified C++14 source files into the specified object file.
 */
function compile(
  filePath: string,
  sourcePaths: ImmList<string>,
  helpers: Helpers,
): Promise<BuildResult> {
  const depFilePath = pathWithoutExt(filePath) + '.d';
  return helpers.spawn('clang++', [
    '-c', '-o', filePath, '-Wall', '-std=c++14', '-fcolor-diagnostics',
    '-MMD', '-MF', depFilePath, '-I', '/usr/local/include',
  ].concat(sourcePaths.toArray())).then(result => {
    if (result !== 'built') {
      return Promise.resolve({result, dynamicDependencies: immutable.List()});
    }
    return readFilePromise(depFilePath, 'utf8').then(content => {
      // Ain't got no time for a true parser.
      const dynamicDependencies = immutable.List(content.split(/(?:\n| )/)
        .filter(chunk => chunk.endsWith('.h'))
        .map(filePath => path.normalize(filePath)));
      return {result, dynamicDependencies};
    });
  });
}

/**
 * Link the specified binary object files into the specified executable file.
 */
function link(
  filePath: string,
  sourcePaths: ImmList<string>,
  helpers: Helpers,
): Promise<BuildResult> {
  return helpers.spawn('clang++', [
    '-o', filePath, '-framework', 'OpenGL', '-Wall', '-std=c++14', '-lglew',
    '-lglfw3', '-fcolor-diagnostics', '-L', '/usr/local/lib',
  ].concat(sourcePaths.toArray())).then(result => {
    return {result, dynamicDependencies: immutable.List()};
  });
}

/**
 * Entry point: build the lists of all the files we need to build.
 */
cli(opts => {
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
      return chain(
        adjacencyList.add(fileAdj, sourcePath, objectPath),
        adj => adjacencyList.add(adj, objectPath, 'gl-demo'),
      );
    }, adjacencyList.empty())
  );
  const fileBuilders = sourceObjectPairs.reduce((builders, objectFilePath) => {
    return builders.set(objectFilePath, compile);
  }, immutable.Map()).set('gl-demo', link);
  return {...opts, fileAdjacencyList, fileBuilders};
});
