/* @flow */

'use strict';

import type {AgentEvent, FilePath} from './agent-event';
import type {FileAdjacencyList} from './file-adjacency-list';
import type {Process} from './process';
import type {ImmList, ImmMap, ImmSet} from 'immutable';

import fileAdjacencyList from './file-adjacency-list'
import immutable from 'immutable';

type StaleFiles = ImmSet<FilePath>;

type AgentCLIConfig = {
  // Terminate the agent once everything is built.
  once: boolean,
  // Print commands.
  verbose: boolean,
};

export type AgentConfig = AgentCLIConfig & {
  // What file is used to build other files.
  fileAdjacencyList: FileAdjacencyList,
  // What should be used to build a each particular file.
  fileBuilders: ImmMap<FilePath, (
    filePath: FilePath,
    sourceFilePaths: ImmList<FilePath>,
  ) => Process>,
};

export type AgentState = {
  // All the files we want to update.
  staleFiles: StaleFiles,
  // What files are being updated right now.
  updatingFiles: ImmSet<FilePath>,
};

/**
 * Return the initial state given a particular configuration.
 */
export function initialize(
  config: AgentConfig,
): AgentState {
  return Object.freeze({
    staleFiles: immutable.Set(),
    updatingFiles: immutable.Set(),
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
  return immutable.Set();
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
  const {updatingFiles} = state;
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
  return immutable.Map();
}
