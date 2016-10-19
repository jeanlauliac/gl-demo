/* @flow */

'use strict';

import type {FileStatus} from './file-status';

export type FilePath = string;

// Describe something that happened in the Upd agent.
export type Event =
  {
    // The path of the directory that was created.
    directoryPath: FilePath,
    // A directory was created successfully.
    type: 'create-directory-success',
  } |
  {
    directoryPath: FilePath,
    error: Error,
    type: 'create-directory-failure',
  } |
  {
    // Return code of the process. Process was successful if this is zero.
    code: number,
    // File being updated by this process.
    targetPath: string,
    // Signal that killed the process, if any.
    signal: string,
    // An update process finished.
    type: 'update-process-exit',
  };

export type DispatchEvent = (event: Event) => void;
