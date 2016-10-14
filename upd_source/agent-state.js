/* @flow */

'use strict';

import type {Event, DispatchEvent, FilePath} from './agent-event';
import type {FileAdjacencyList} from './file_adjacency_list';
import type {CreateDirectory, StatusesByDirectory} from './directories';
import type {Process} from './process';
import type {UpdateProcesses} from './update_processes';
import type {ChildProcess} from 'child_process';

import * as adjacency_list from './adjacency_list';
import nullthrows from './nullthrows';
import * as directories from './directories';
import * as update_processes from './update_processes';
import * as immutable from 'immutable';
import {dirname} from 'path';

type FileSet = immutable.Set<FilePath>;
type FileList = immutable.List<FilePath>;

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
  fileBuilders: immutable.Map<FilePath, (
    filePath: FilePath,
    sourceFilePaths: immutable.Set<FilePath>,
  ) => Process>,
};

export type AgentState = {
  // All the directories that exist or that we are creating.
  statusesByDirectory: StatusesByDirectory,
  // All the files we want to update.
  staleFiles: FileSet,
  // Processes updating files right now.
  updateProcesses: UpdateProcesses,
};

/**
 * Given the stale files, start up processes to build them limited by the
 * concurrency level.
 */
function startUpProcesses(
  updateProcesses: immutable.Map<FilePath, ChildProcess>,
  config: AgentConfig,
  staleFiles: FileSet,
  dispatch: DispatchEvent,
): immutable.Map<FilePath, ChildProcess> {
  return immutable.Map();
  // staleFiles.filter(filePath => {
  //   return adjacency_list.precedingSeq(config.fileAdjacencyList, filePath)
  //     .every((_, predFilePath) => !staleFiles.has(predFilePath)) &&
  //     existingDirectories.has(dirname(filePath))
  //   }).toList(),
}

/**
 * Return the initial set of files that are stale, and start up the first
 * updating processes.
 */
export function create(
  config: AgentConfig,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
): AgentState {
  const staleFiles = immutable.Set(config.fileBuilders.keys());
  const existingDirectories = immutable.Set();
  return Object.freeze({
    statusesByDirectory: directories.create({
      targetPaths: staleFiles,
      dispatch,
      createDirectory,
    }),
    staleFiles,
    updateProcesses: update_processes.create(),
  });
}

/**
 * Identify which files are stale given an event that just hapenned. If we just
 * started, all files that can be built are considered stale.
 */
function updateStaleFiles(
  config: AgentConfig,
  staleFiles: FileSet,
  event: Event,
): FileSet {
  switch (event.type) {
    case 'process-exit':
      if (event.code === 0) {
        return staleFiles.delete(event.key);
      }
      break;
  }
  return staleFiles;
}

// function updateUpdatingFiles(
//   config: AgentConfig,
//   updatingFiles: FileList,
//   event: Event,
// ): FileList {
//   switch (event.type) {
//     case 'process_exit':
//       const index = updatingFiles.indexOf(event.key);
//       if (index >= 0) {
//         return updatingFiles.delete(index);
//       }
//       break;
//   }
//   return updatingFiles;
// }

/**
 * Decide the next step needed to update the state.
 */
export function update(
  state: AgentState,
  config: AgentConfig,
  event: Event,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
): AgentState {
  let {
    statusesByDirectory,
    staleFiles,
    updateProcesses,
  } = state;
  //const staleFiles = updateStaleFiles(config, state.staleFiles, event);
  //const updatingFiles = updateUpdatingFiles(config, state.updatingFiles, event);
  return Object.freeze({
    statusesByDirectory: directories.update({
      statusesByDirectory,
      dispatch,
      createDirectory,
      event,
      targetPaths: staleFiles,
    }),
    staleFiles,
    updateProcesses: update_processes.update({
      updateProcesses,
    })
  });
}

/**
 * Return the list of processes that should be running right now, indexed
 * by the name of the file being built.
 */
// export function getProcesses(
//   config: AgentConfig,
//   state: AgentState,
// ): immutable.Map<FilePath, Process> {
//   const adjList = config.fileAdjacencyList;
//   return state.updatingFiles.take(config.cliOpts.concurrency)
//     .reduce((processes, filePath) => {
//       const builder = nullthrows(config.fileBuilders.get(filePath));
//       const sources = immutable.Set(
//         adjacency_list.precedingSeq(adjList, filePath).keys()
//       );
//       return processes.set(filePath, builder(filePath, sources));
//     }, immutable.Map());
// }
