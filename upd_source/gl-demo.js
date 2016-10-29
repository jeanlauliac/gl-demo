/* @flow */

'use strict';

import type {FilePath} from './file_path';
import type {ProcessDesc} from './process_desc';

import * as adjacency_list from './adjacency_list';
import chain from './chain';
import cli from './cli';
import * as file_path from './file_path';
import * as process_desc from './process_desc';
import glob from 'glob';
import * as immutable from 'immutable';
import path from 'path';

/**
 * Return the same file directory and name stripped of its extension.
 */
function pathWithoutExt(filePath: FilePath): FilePath {
  const ext = path.extname(filePath);
  return file_path.create(filePath.substr(0, filePath.length - ext.length));
}

/**
 * Return a descriptor of the process that compiles the specified C++14 source
 * files into the specified object file.
 */
function compile(
  filePath: FilePath,
  sourcePaths: immutable.Set<FilePath>,
): ProcessDesc {
  const depFilePath = pathWithoutExt(filePath) + '.d';
  return process_desc.create('clang++', immutable.List([
    '-c', '-o', filePath, '-Wall', '-std=c++14', '-fcolor-diagnostics',
    '-MMD', '-MF', depFilePath, '-I', '/usr/local/include',
  ]).concat(
    /* $FlowIssue: should be covariant */
    (sourcePaths: immutable.Set<string>),
  ));
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
  filePath: FilePath,
  sourcePaths: immutable.Set<FilePath>,
): ProcessDesc {
  return process_desc.create('clang++', immutable.List([
    '-o', filePath, '-framework', 'OpenGL', '-Wall', '-std=c++14',
    '-lglew', '-lglfw3', '-fcolor-diagnostics', '-L', '/usr/local/lib',
  ]).concat(
    /* $FlowIssue: should be covariant */
    (sourcePaths: immutable.Set<string>),
  ));
}

/**
 * Entry point: build the lists of all the files we need to build.
 */
cli(cliOpts => {
  const sourceFiles = immutable.List(['main.cpp'])
    .concat(glob.sync('glfwpp/*.cpp'))
    .concat(glob.sync('glpp/*.cpp'))
    .concat(glob.sync('ds/*.cpp'))
    .map(p => file_path.create(p));
  const sourceObjectPairs = sourceFiles.map(sourceFilePath => {
    const barePath = path.relative('.', pathWithoutExt(sourceFilePath));
    const objectFilePath = file_path.create(
      path.join('.upd_cache', barePath + '.o'),
    );
    return [sourceFilePath, objectFilePath];
  });
  const fileAdjacencyList = (
    sourceObjectPairs.reduce((fileAdj, [sourcePath, objectPath]) => {
      return chain([
        fileAdj => adjacency_list.add(fileAdj, sourcePath, objectPath),
        fileAdj => adjacency_list.add(fileAdj, objectPath, file_path.create('gl-demo')),
      ], fileAdj);
    }, adjacency_list.empty())
  );
  const fileBuilders = sourceObjectPairs.reduce((builders, pair) => {
    return builders.set(pair[1], compile);
  }, immutable.Map()).set(file_path.create('gl-demo'), link);
  return {cliOpts, fileAdjacencyList, fileBuilders};
});
