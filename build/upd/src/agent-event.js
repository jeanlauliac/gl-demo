/* @flow */

'use strict';

import type {FilePath} from './file_path';
import type {UpdateProcessDesc} from './update_process_desc';

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
    targetPath: FilePath,
    // Data output on stderr.
    stderr: Buffer,
    // Data output on stdout.
    stdout: Buffer,
    // Signal that killed the process, if any.
    signal: string,
    // An update process finished.
    type: 'update-process-exit',
    // Desc that originated this process.
    updateDesc: UpdateProcessDesc,
  } |
  {
    content: string,
    type: 'dynamic-dependencies-file-read',
  } |
  {
    filePath: FilePath,
    type: 'chokidar-file-changed',
  } |
  {
    type: 'update-requested',
  };

export type DispatchEvent = (event: Event) => void;
