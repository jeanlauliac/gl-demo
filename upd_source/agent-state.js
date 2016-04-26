/* @flow */

'use strict';

import type {AgentEvent, FilePath} from './agent-event';
import type {FileAdjacencyList} from './file-adjacency-list';
import type {Process} from './process';

import fileAdjacencyList from './file-adjacency-list'
import immutable from 'immutable';

type StaleFiles = ImmSet<FilePath>;

export type AgentState = {
  // All the files we want to update.
  staleFiles: StaleFiles,
  // What is the process currently building a file.
  processByFile: ImmMap<FilePath, Process>,
  // The processes currently running. They are in the order of their creation.
  processes: ImmList<Process>,
};

/**
 * Identify which files are stale given an event that just hapenned. If we just
 * started, all files that can be built are considered stale.
 */
function updateStaleFiles(
  config: AgentConfig,
  staleFiles: StaleFiles,
  event: AgentEvent,
): StaleFiles {
  if (event.type === 'start') {
    return immutable.Set(config.fileBuilders.keySeq().toList());
  }
  return staleFiles;
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
  return Object.freeze({staleFiles});
}
