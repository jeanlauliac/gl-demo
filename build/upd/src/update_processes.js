/* @flow */

'use strict';

import type {Spawn} from './Agent';
import type {DispatchEvent, Event} from './agent-event';
import type {AgentConfig, FileSet} from './agent-state';
import type {ChildProcess} from 'child_process';
import type {StatusesByDirectory} from './directories';
import type {FilePath} from './file_path';

import * as adjacency_list from './adjacency_list';
import * as directories from './directories';
import * as file_path from './file_path';
import * as immutable from 'immutable';
import {dirname} from 'path';

export type UpdateProcesses = immutable.Map<FilePath, ChildProcess>;

function spawnNextProcesses(props: {
  config: AgentConfig,
  dispatch: DispatchEvent,
  spawn: Spawn,
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
  updateProcesses: UpdateProcesses,
}): UpdateProcesses {
  const {config, statusesByDirectory, targetPaths, updateProcesses} = props;
  const adjList = config.fileAdjacencyList;
  const candidatePaths = targetPaths.toSeq().filter(filePath => {
    if (updateProcesses.has(filePath)) {
      return false;
    }
    const dirPath = file_path.dirname(filePath);
    if (!directories.doesExist(dirPath, statusesByDirectory)) {
      return false;
    }
    const preds = adjacency_list.precedingSeq(adjList, filePath);
    return preds.every((_, predFilePath) => !targetPaths.has(predFilePath));
  });
  const scheduledPaths = candidatePaths.take(
    config.cliOpts.concurrency - updateProcesses.size,
  );
  return scheduledPaths.reduce((updateProcesses, filePath) => {
    const builder = config.fileBuilders.get(filePath);
    if (builder == null) {
      return updateProcesses;
    }
    const sources = immutable.Set(
      adjacency_list.precedingSeq(adjList, filePath).keys(),
    );
    const processDesc = builder(filePath, sources);
    const process = props.spawn(
      processDesc.command,
      processDesc.args.toArray(),
    );
    const stdouts = [];
    const stderrs = [];
    process.stdout.on('data', data => stdouts.push(data));
    process.stderr.on('data', data => stderrs.push(data));
    process.on('exit', (code, signal) => {
      props.dispatch({
        code,
        signal,
        stderr: Buffer.concat(stderrs),
        stdout: Buffer.concat(stdouts),
        targetPath: filePath,
        type: 'update-process-exit',
      });
    });
    return updateProcesses.set(filePath, process);
  }, updateProcesses);
}

export function create(props: {
  config: AgentConfig,
  dispatch: DispatchEvent,
  spawn: Spawn,
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
}): UpdateProcesses {
  return spawnNextProcesses({
    ...props,
    updateProcesses: immutable.Map(),
  });
}

function updateForEvent(props: {
  event: Event,
  updateProcesses: UpdateProcesses,
}): UpdateProcesses {
  const {event, updateProcesses} = props;
  switch (event.type) {
    case 'update-process-exit':
      if (event.code === 0) {
        return updateProcesses.delete(event.targetPath);
      }
  }
  return updateProcesses;
}

export function update(props: {
  config: AgentConfig,
  dispatch: DispatchEvent,
  event: Event,
  spawn: Spawn,
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
  updateProcesses: UpdateProcesses,
}): UpdateProcesses {
  return spawnNextProcesses({
    ...props,
    updateProcesses: updateForEvent(props),
  });
}
