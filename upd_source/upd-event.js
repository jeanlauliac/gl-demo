/* @flow */

'use strict';

export type FilePath = string;

// Describe something that happened in the Upd agent.
export type UpdEvent = {
  programFilePath: FilePath,
  sourceFilePaths: immutable._Iterable_Indexed<FilePath>,
  type: 'start',
} | {
  filePath: FilePath,
  type: 'file-update-failed',
} | {
  depPaths: immutable._Iterable_Indexed<FilePath>,
  filePath: FilePath,
  type: 'file-updated',
} | {
  filePath: FilePath,
  type: 'file-changed',
};
