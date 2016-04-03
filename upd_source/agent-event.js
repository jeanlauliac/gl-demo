/* @flow */

'use strict';

import type {FileStatus} from './file-status';

export type FilePath = string;

// Describe something that happened in the Upd agent.
export type AgentEvent = {
  // Process the current, initial state, potentially starting some processes.
  type: 'start',
};
