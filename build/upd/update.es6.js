/* @flow */

'use strict';

import type {FilePath} from './src/file_path';
import type {ProcessDesc} from './src/process_desc';

import * as adjacency_list from './src/adjacency_list';
import chain from './src/chain';
import upd from './src';
import * as file_path from './src/file_path';
import * as process_desc from './src/process_desc';
import * as update_process_desc from './src/update_process_desc';
import glob from 'glob';
import * as immutable from 'immutable';
import path from 'path';

function pathWithoutExt(filePath: FilePath): FilePath {
  const ext = path.extname(filePath);
  return file_path.create(filePath.substr(0, filePath.length - ext.length));
}

function transformJS(filePath, sourcePaths) {
  return update_process_desc.create(
    process_desc.create(
      path.resolve(__dirname, '../../node_modules/.bin/babel'),
      immutable.List([
        '-o', filePath, '--retain-lines',
      ]).concat(
        /* $FlowIssue: should be covariant */
        (sourcePaths: immutable.Set<string>),
      ),
    ),
  );
}

upd(cliOpts => {
  const sourceFiles = immutable.List(
    glob.sync(path.join(__dirname, 'src/*.js'))
  ).map(p => file_path.create(p));
  const sourceResultPairs = sourceFiles.map(sourceFilePath => {
    const barePath = path.relative(path.join(__dirname, 'src'), sourceFilePath);
    const resultFilePath = file_path.create(
      path.join(__dirname, 'dist', barePath),
    );
    return [sourceFilePath, resultFilePath];
  });
  const fileAdjacencyList =
    sourceResultPairs.reduce((fileAdj, [sourcePath, objectPath]) => {
      return adjacency_list.add(fileAdj, sourcePath, objectPath);
    }, adjacency_list.empty());
  const fileBuilders = sourceResultPairs.reduce((builders, pair) => {
    return builders.set(pair[1], transformJS);
  }, immutable.Map());
  return {cliOpts, fileAdjacencyList, fileBuilders};
});
