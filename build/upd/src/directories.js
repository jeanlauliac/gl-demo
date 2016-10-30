/* @flow */

'use strict';

import type {FilePath} from './file_path';
import type {DispatchEvent, Event} from './agent-event';

import * as file_path from './file_path';
import * as immutable from 'immutable';
import path from 'path';

type FileSet = immutable.Set<FilePath>;
type DirectoryOperation = 'create' | 'none';
type DirectoryStatus = {
  operation: DirectoryOperation,
  error: ?Error,
};

export type StatusesByDirectory = immutable.Map<FilePath, DirectoryStatus>;

export function doesExist(
  dirPath: FilePath,
  statsByDir: StatusesByDirectory,
): boolean {
  if (immutable.is(file_path.dirname(dirPath), dirPath)) {
    return true;
  }
  const stat = statsByDir.get(dirPath);
  return stat != null && stat.operation === 'none' && stat.error == null;
}

/**
 * Get the set of directories we should create next. `targetPaths` describes
 * all the filepaths which enclosing directories we should create recursively.
 */
export function nextOnes(props: {
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
}): FileSet {
  const {statusesByDirectory} = props;
  return props.targetPaths.reduce((directories, filePath) => {
    if (doesExist(file_path.dirname(filePath), statusesByDirectory)) {
      return directories;
    }
    while (!doesExist(file_path.dirname(filePath), statusesByDirectory)) {
      filePath = file_path.dirname(filePath);
    }
    return directories.add(filePath);
  }, immutable.Set());
}

const MAX_DIRECTORY_CONCURRENCY = 4;
export type CreateDirectory = (directoryPath: FilePath) => Promise<void>;

/**
 * Start creating missing directories when possible. Return the set of directory
 * paths being created right now (pending).
 */
export function updateMissing(props: {
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
}): StatusesByDirectory {
  const {statusesByDirectory} = props;
  const dirsToCreate = nextOnes(props)
    .filter(dirPath => !statusesByDirectory.has(dirPath))
    .take(MAX_DIRECTORY_CONCURRENCY);
  return statusesByDirectory.withMutations(dirs => {
    dirsToCreate.forEach(directoryPath => {
      dirs.set(directoryPath, {operation: 'create', error: null});
      props.createDirectory(directoryPath).then(() => {
        props.dispatch({directoryPath, type: 'create-directory-success'});
      }, error => {
        props.dispatch(
          {directoryPath, error, type: 'create-directory-failure'},
        );
      });
    });
  });
}

/**
 * Create the initial bulk of directories. Return the list of directories
 * being created right now.
 */
export function create(props: {
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
}): StatusesByDirectory {
  const assumedDirs = immutable.Map([
    //[file_path.create('.'), {operation: 'none', error: null}],
  ]);
  return updateMissing({...props, statusesByDirectory: assumedDirs});
}

function updateForEvent(props: {
  statusesByDirectory: StatusesByDirectory,
  event: Event,
}): StatusesByDirectory {
  let {event, statusesByDirectory} = props;
  switch (event.type) {
    case 'create-directory-success':
      return statusesByDirectory.set(
        event.directoryPath,
        {operation: 'none', error: null},
      );
    case 'create-directory-failure':
      // $FlowIssue: `code` is a custom field for fs-related errors.
      if (event.error.code === 'EEXIST') {
        return statusesByDirectory.set(
          event.directoryPath,
          {operation: 'none', error: null},
        );
      }
      return statusesByDirectory.set(
        event.directoryPath,
        {operation: 'none', error: event.error},
      );
  }
  return statusesByDirectory;
}

/**
 * Update the list of pending directories given a particular event. Return the
 * new list of directories being created right now.
 */
export function update(props: {
  statusesByDirectory: StatusesByDirectory,
  targetPaths: FileSet,
  dispatch: DispatchEvent,
  createDirectory: CreateDirectory,
  event: Event,
}): StatusesByDirectory {
  return updateMissing({
    ...props,
    statusesByDirectory: updateForEvent(props),
  });
}
