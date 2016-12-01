/* @flow */

'use strict';

import type {FilePath} from './upd/dist/file_path';
import type {ProcessDesc} from './upd/dist/process_desc';

import * as adjacency_list from './upd/dist/adjacency_list';
import chain from './upd/dist/chain';
import upd from './upd/dist';
import * as file_path from './upd/dist/file_path';
import * as process_desc from './upd/dist/process_desc';
import * as update_process_desc from './upd/dist/update_process_desc';
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
function compile(filePath, sourcePaths) {
  const depFilePath = file_path.create(pathWithoutExt(filePath) + '.d');
  return update_process_desc.create(
    process_desc.create('clang++', immutable.List([
      '-c', '-o', filePath, '-Wall', '-std=c++14', '-fcolor-diagnostics',
      '-MMD', '-MF', depFilePath, '-I', '/usr/local/include',
    ]).concat(
      /* $FlowIssue: should be covariant */
      (sourcePaths: immutable.Set<string>),
    )),
    depFilePath,
  );
}

/**
 * Return a descriptor of the process that links the specified binary object
 * files into the specified executable file.
 */
function link(filePath, sourcePaths) {
  return update_process_desc.create(
    process_desc.create('clang++', immutable.List([
      '-o', filePath, '-framework', 'OpenGL', '-Wall', '-std=c++14',
      '-lglew', '-lglfw3', '-fcolor-diagnostics', '-L', '/usr/local/lib',
    ]).concat(
      /* $FlowIssue: should be covariant */
      (sourcePaths: immutable.Set<string>),
    )),
  );
}

function embedResource(filePath, sourcePaths) {
  return update_process_desc.create(
    process_desc.create(
      path.resolve(__dirname, 'embed-resource.js'),
      immutable.List([filePath]).concat((sourcePaths: immutable.Set<string>)),
    ),
  );
}

/**
 * Entry point: build the lists of all the files we need to build.
 */
upd(cliOpts => {
  const topDir = path.resolve(__dirname, '..');
  const resourceFiles = immutable.List(glob.sync(
    path.resolve(topDir, 'resources/**/*.@(vs|fs)'),
  ));
  const resourceTuples = resourceFiles.map(resourceFilePath => {
    const barePath = path.relative(topDir, resourceFilePath);
    const sourceFilePath = file_path.create(
      path.join(topDir, '.upd', 'cache', barePath + '.cpp'),
    );
    const objectFilePath = file_path.create(
      path.join(topDir, '.upd', 'cache', barePath + '.o'),
    );
    return [resourceFilePath, sourceFilePath, objectFilePath];
  });
  const sourceFiles = immutable.List(['main.cpp'])
    .concat(glob.sync('glfwpp/*.cpp'))
    .concat(glob.sync('glpp/*.cpp'))
    .concat(glob.sync('ds/*.cpp'))
    .map(p => file_path.create(p));
  const sourceObjectPairs = sourceFiles.map(sourceFilePath => {
    const barePath = path.relative('.', pathWithoutExt(sourceFilePath));
    const objectFilePath = file_path.create(
      path.join(__dirname, '..', '.upd', 'cache', barePath + '.o'),
    );
    return [sourceFilePath, objectFilePath];
  }).concat(resourceTuples.map(t => t.slice(1)));
  const distBinPath = file_path.create('dist/gl-demo');
  const fileAdjacencyList = chain([
    fileAdj =>
      sourceObjectPairs.reduce((fileAdj, [sourcePath, objectPath]) => {
        return chain([
          fileAdj => adjacency_list.add(fileAdj, sourcePath, objectPath),
          fileAdj => adjacency_list.add(fileAdj, objectPath, distBinPath),
        ], fileAdj);
      }, fileAdj),
    fileAdj =>
      resourceTuples.reduce((fileAdj, [resourceFilePath, sourceFilePath]) => {
        return adjacency_list.add(fileAdj, resourceFilePath, sourceFilePath);
      }, fileAdj),
  ], adjacency_list.empty());
  const fileBuilders = chain([
    builders => sourceObjectPairs.reduce((builders, pair) => {
      return builders.set(pair[1], compile);
    }, builders),
    builders => resourceTuples.reduce((builders, tup) => {
      return builders.set(tup[1], embedResource);
    }, builders),
    builders => builders.set(distBinPath, link),
  ], immutable.Map());
  return {cliOpts, fileAdjacencyList, fileBuilders};
});
