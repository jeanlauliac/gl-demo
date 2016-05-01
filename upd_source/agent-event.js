/* @flow */

'use strict';

import type {FileStatus} from './file-status';

export type FilePath = string;

// Describe something that happened in the Upd agent.
export type AgentEvent = {
  // Return code of the process.
  code: number,
  // The key of the process that exited.
  key: string,
  // Signal that killed the process, if any.
  signal: string,
  // A process that has been returned by `getProcesses` before has exited.
  type: 'process_exit',
};
