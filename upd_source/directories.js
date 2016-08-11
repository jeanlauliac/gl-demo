/* @flow */

'use strict';

import immutable from 'immutable';
import path from 'path';

type FileSet = ImmSet<FilePath>;

/**
 * Get the set of directories we should create next.
 */
export function getPending(
  existingDirectories: FileSet,
  targetPaths: FileSet,
): FileSet {
  return targetPaths.reduce((directories, filePath) => {
    if (existingDirectories.has(path.dirname(filePath))) {
      return directories;
    }
    while (!existingDirectories.has(path.dirname(filePath))) {
      if (path.dirname(filePath) === filePath) {
        return directories;
      }
      filePath = path.dirname(filePath);
    }
    return directories.add(filePath);
  }, immutable.Set());
}

export function create(
  existingDirectories: FileSet,
  staleFiles: FileSet,
  dispatch: DispatchEvent,
) {
  const missingDirs = getMissingDirectories(existingDirectories, staleFiles);
  missingDirs.foreach(() => {

  });
}

export function update(

) {

}
