/* @flow */

'use strict';

import type {Spawn} from './Agent'
import type {Event, DispatchEvent} from './agent-event';
import type {CreateDirectory, StatusesByDirectory} from './directories';
import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';
import type {ProcessDesc} from './process_desc';
import type {UpdateProcesses} from './update_processes';
import type {ChildProcess} from 'child_process';

import * as adjacency_list from './adjacency_list';
import nullthrows from './nullthrows';
import * as directories from './directories';
import * as file_path from './file_path';
import * as update_processes from './update_processes';
import * as immutable from 'immutable';
import {dirname} from 'path';

export type FileSet = immutable.Set<FilePath>;
export type FileList = immutable.List<FilePath>;

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
  ) => ProcessDesc>,
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
 * Return the initial set of files that are stale, and start up the first
 * updating processes.
 */
export function create(props: {
  config: AgentConfig,
  createDirectory: CreateDirectory,
  dispatch: DispatchEvent,
  spawn: Spawn,
}): AgentState {
  const {config, dispatch, createDirectory, spawn} = props;
  const staleFiles = immutable.Set(config.fileBuilders.keys());
  const statusesByDirectory = directories.create({
    targetPaths: staleFiles,
    dispatch,
    createDirectory,
  });
  return Object.freeze({
    statusesByDirectory,
    staleFiles,
    updateProcesses: update_processes.create({
      config,
      dispatch,
      spawn,
      statusesByDirectory,
      targetPaths: staleFiles,
    }),
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
    case 'update-process-exit':
      if (event.code === 0) {
        return staleFiles.delete(event.targetPath);
      }
      break;
  }
  return staleFiles;
}

/**
 * Decide the next step needed to update the state.
 */
export function update(props: {
  config: AgentConfig,
  createDirectory: CreateDirectory,
  dispatch: DispatchEvent,
  spawn: Spawn,
  state: AgentState,
  event: Event,
}): AgentState {
  const {config, dispatch, event, state} = props;
  let {
    statusesByDirectory,
    staleFiles,
    updateProcesses,
  } = state;
  staleFiles = updateStaleFiles(config, staleFiles, event);
  statusesByDirectory = directories.update({
    statusesByDirectory,
    dispatch,
    createDirectory: props.createDirectory,
    event,
    targetPaths: staleFiles,
  });
  return Object.freeze({
    staleFiles,
    statusesByDirectory,
    updateProcesses: update_processes.update({
      config,
      dispatch,
      event,
      updateProcesses,
      spawn: props.spawn,
      statusesByDirectory,
      targetPaths: staleFiles,
    }),
  });
}
