/* @flow */

'use strict';

import type {Spawn} from './Agent'
import type {AdjacencyList} from './adjacency_list';
import type {Event, DispatchEvent} from './agent-event';
import type {CreateDirectory, StatusesByDirectory} from './directories';
import type {DynamicDependencies} from './dynamic_dependencies';
import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';
import type {UpdateProcessDesc} from './update_process_desc';
import type {UpdateProcesses} from './update_processes';
import type {ChildProcess} from 'child_process';

import * as adjacency_list from './adjacency_list';
import * as directories from './directories';
import * as dynamic_dependencies from './dynamic_dependencies';
import nullthrows from './nullthrows';
import * as file_path from './file_path';
import * as scheduled_files from './scheduled_files';
import * as update_processes from './update_processes';
import * as immutable from 'immutable';
import {dirname} from 'path';
import {descendantsOf} from './file_adjacency_list';

export type FileSet = immutable.Set<FilePath>;
export type FileList = immutable.List<FilePath>;

export type AgentCLIOptions = {
  // What should the process do.
  command: string,
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
  ) => UpdateProcessDesc>,
};

export type AgentState = {
  // Dependencies detected while we build more files, in addition to the
  // file adjacencies.
  dynamicDependencies: DynamicDependencies,
  // All the files we want to update right now.
  scheduledFiles: FileSet,
  // All the directories that exist or that we are creating.
  statusesByDirectory: StatusesByDirectory,
  // All the files we need to update if requested.
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
  const scheduledFiles = scheduled_files.create();
  return Object.freeze({
    dynamicDependencies: dynamic_dependencies.create(),
    scheduledFiles,
    statusesByDirectory,
    staleFiles,
    updateProcesses: update_processes.create({
      config,
      dispatch,
      spawn,
      statusesByDirectory,
      scheduledFiles,
      staleFiles,
    }),
  });
}

/**
 * Identify which files are stale given an event that just happened. If we just
 * started, all files that can be built are considered stale.
 */
function updateStaleFiles(props: {
  config: AgentConfig,
  dynamicDependencies: DynamicDependencies,
  staleFiles: FileSet,
  event: Event,
}): FileSet {
  const {config, event, dynamicDependencies, staleFiles} = props;
  switch (event.type) {
    case 'update-process-exit':
      if (event.code === 0) {
        return staleFiles.delete(event.targetPath);
      }
      break;
    case 'chokidar-file-changed':
      if (!adjacency_list.precedingSeq(
        config.fileAdjacencyList,
        event.filePath,
      ).isEmpty()) {
        break;
      }
      return staleFiles.union(
        descendantsOf(config.fileAdjacencyList, event.filePath),
        descendantsOf(dynamicDependencies, event.filePath),
      );
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
    dynamicDependencies,
    scheduledFiles,
    statusesByDirectory,
    staleFiles,
    updateProcesses,
  } = state;
  dynamicDependencies = dynamic_dependencies.update({
    config,
    dispatch,
    event,
    dynamicDependencies,
  });
  staleFiles = updateStaleFiles({
    config,
    dynamicDependencies,
    staleFiles,
    event,
  });
  scheduledFiles = scheduled_files.update({
    config,
    scheduledFiles,
    staleFiles,
    event,
  });
  statusesByDirectory = directories.update({
    statusesByDirectory,
    dispatch,
    createDirectory: props.createDirectory,
    event,
    targetPaths: scheduledFiles,
  });
  return Object.freeze({
    dynamicDependencies,
    scheduledFiles,
    staleFiles,
    statusesByDirectory,
    updateProcesses: update_processes.update({
      config,
      dispatch,
      event,
      updateProcesses,
      spawn: props.spawn,
      statusesByDirectory,
      scheduledFiles,
      staleFiles,
    }),
  });
}
