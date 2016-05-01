/* @flow */

'use strict';

import type {AgentEvent, FilePath} from './agent-event';
import type {FileAdjacencyList} from './file-adjacency-list';
import type {Process} from './process';
import type {ImmList, ImmMap, ImmSet} from 'immutable';

import * as adjacencyList from './adjacency-list';
import nullthrows from './nullthrows';
import immutable from 'immutable';

type StaleFiles = ImmSet<FilePath>;

export type AgentCLIOptions = {
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
  staleFiles: StaleFiles,
  // What files are being updated right now.
  updatingFiles: ImmSet<FilePath>,
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
    }),
  });
}

/**
 * Identify which files are stale given an event that just hapenned. If we just
 * started, all files that can be built are considered stale.
 */
function updateStaleFiles(
  config: AgentConfig,
  staleFiles: StaleFiles,
  event: AgentEvent,
): StaleFiles {
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
  updatingFiles: ImmSet<FilePath>,
  event: AgentEvent,
): ImmSet<FilePath> {
  switch (event.type) {
    case 'process_exit':
      return updatingFiles.delete(event.key);
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
  return state.updatingFiles.reduce((processes, filePath) => {
    const builder = nullthrows(config.fileBuilders.get(filePath));
    const sources = immutable.Set(
      adjacencyList.precedingSeq(adjList, filePath).keys()
    );
    return processes.set(filePath, builder(filePath, sources));
  }, immutable.Map());
}
