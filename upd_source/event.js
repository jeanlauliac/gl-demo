/* @flow */

'use strict';

export type FilePath = string;

export type Event = {
  type: 'start',
  sourceFilePaths: immutable._Iterable_Indexed<FilePath>,
  programFilePath: FilePath,
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
