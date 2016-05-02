/* @flow */

'use strict';

import type {AgentEvent, FilePath} from './agent-event';
import type {FileAdjacencyList} from './file-adjacency-list';
import type {Process} from './process';
import type {ImmList, ImmMap, ImmSet} from 'immutable';

import * as adjacencyList from './adjacency-list';
import nullthrows from './nullthrows';
import immutable from 'immutable';

type FileSet = ImmSet<FilePath>;
type FileList = ImmList<FilePath>;

export type AgentCLIOptions = {
  // Maximum number of sub-processes it should run at the same time.
  concurrency: number,
  // Terminate the agent once everything is built.
  once: boolean,
  // Print commands.
  verbose: boolean,
};

export type AgentConfig = {
  cliOpts: AgentCLIOptions,
  // What file is used to build other files.
  fileAdjacencyList: FileAdjacencyList,
  // What should be used to build a each particular file.
  fileBuilders: ImmMap<FilePath, (
    filePath: FilePath,
    sourceFilePaths: ImmSet<FilePath>,
  ) => Process>,
};

export type AgentState = {
  // All the files we want to update.
  staleFiles: FileSet,
  // What files are being updated next. The order indicates what file
  // should be updated first.
  updatingFiles: FileList,
};

/**
 * Return the initial set of files that are stale, and which are to update now.
 */
export function initialize(
  config: AgentConfig,
): AgentState {
  const staleFiles = immutable.Set(config.fileBuilders.keys());
  return Object.freeze({
    staleFiles,
    updatingFiles: staleFiles.filter(filePath => {
      return adjacencyList.precedingSeq(config.fileAdjacencyList, filePath)
        .every((_, predFilePath) => !staleFiles.has(predFilePath))
    }).toList(),
  });
}

/**
 * Identify which files are stale given an event that just hapenned. If we just
 * started, all files that can be built are considered stale.
 */
function updateStaleFiles(
  config: AgentConfig,
  staleFiles: FileSet,
  event: AgentEvent,
): FileSet {
  switch (event.type) {
    case 'process_exit':
      if (event.code === 0) {
        return staleFiles.delete(event.key);
      }
      break;
  }
  return staleFiles;
}

function updateUpdatingFiles(
  config: AgentConfig,
  updatingFiles: FileList,
  event: AgentEvent,
): FileList {
  switch (event.type) {
    case 'process_exit':
      const index = updatingFiles.indexOf(event.key);
      if (index >= 0) {
        return updatingFiles.delete(index);
      }
      break;
  }
  return updatingFiles;
}

/**
 * Decide the next step needed to update the state.
 */
export function update(
  config: AgentConfig,
  state: AgentState,
  event: AgentEvent,
): AgentState {
  const staleFiles = updateStaleFiles(config, state.staleFiles, event);
  const updatingFiles = updateUpdatingFiles(config, state.updatingFiles, event);
  return Object.freeze({staleFiles, updatingFiles});
}

/**
 * Return the list of processes that should be running right now, indexed
 * by the name of the file being built.
 */
export function getProcesses(
  config: AgentConfig,
  state: AgentState,
): ImmMap<FilePath, Process> {
  const adjList = config.fileAdjacencyList;
  return state.updatingFiles.take(config.cliOpts.concurrency)
    .reduce((processes, filePath) => {
      const builder = nullthrows(config.fileBuilders.get(filePath));
      const sources = immutable.Set(
        adjacencyList.precedingSeq(adjList, filePath).keys()
      );
      return processes.set(filePath, builder(filePath, sources));
    }, immutable.Map());
}
