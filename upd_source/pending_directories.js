/* @flow */

'use strict';

import type {DispatchEvent, Event, FilePath} from './agent-event';

import * as immutable from 'immutable';
import path from 'path';

type FileSet = immutable.Set<FilePath>;

function doesExist(
  dirPath: FilePath,
  existingDirectories: FileSet,
): boolean {
  return (
    path.dirname(dirPath) === dirPath ||
    existingDirectories.has(dirPath)
  );
}

/**
 * Get the set of directories we should create next. `targetPaths` describes
 * all the filepaths which enclosing directories we should create recursively.
 */
export function nextOnes(props: {
  existingDirectories: FileSet,
  targetPaths: FileSet,
}): FileSet {
  const {existingDirectories} = props;
  return props.targetPaths.reduce((directories, filePath) => {
    if (doesExist(filePath, existingDirectories)) {
      return directories;
    }
    while (!doesExist(path.dirname(filePath), existingDirectories)) {
      filePath = path.dirname(filePath);
    }
    return directories.add(filePath);
  }, immutable.Set());
}

const MAX_DIRECTORY_CONCURRENCY = 4;
export type CreateDirectory = (directoryPath: string) => Promise<void>;

/**
 * Start creating missing directories when possible. Return the set of directory
 * paths being created right now (pending).
 */
export function updateMissing(props: {
  existingDirectories: FileSet,
  pendingDirectories: FileSet,
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
}): FileSet {
  const dirsToCreate = nextOnes(props)
    .subtract(props.pendingDirectories)
    .take(MAX_DIRECTORY_CONCURRENCY);
  dirsToCreate.forEach(directoryPath => {
    props.createDirectory(directoryPath).then(() => {
      props.dispatch({directoryPath, type: 'directory-created'});
    }, error => {
      process.nextTick(() => {throw error;});
    });
  });
  return props.pendingDirectories.union(dirsToCreate);
}

/**
 * Create the initial bulk of directories. Return the list of directories
 * being created right now.
 */
export function create(props: {
  existingDirectories: FileSet,
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
}): FileSet {
  return updateMissing({...props, pendingDirectories: immutable.Set()});
}

/**
 * Update the list of pending directories given a particular event. Return the
 * new list of directories being created right now.
 */
export function update(props: {
  existingDirectories: FileSet,
  pendingDirectories: FileSet,
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
  event: Event,
}): FileSet {
  let {event, pendingDirectories} = props;
  switch (event.type) {
    case 'directory-created':
      pendingDirectories = pendingDirectories.remove(event.directoryPath);
      break;
  }
  return updateMissing({...props, pendingDirectories});
}
