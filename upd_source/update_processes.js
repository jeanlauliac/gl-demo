/* @flow */

'use strict';

import type {FilePath} from './agent-event';
import type {ChildProcess} from 'child_process';

import * as immutable from 'immutable';

export type UpdateProcesses = immutable.Map<FilePath, ChildProcess>;

export function create(): UpdateProcesses {
  return immutable.Map();
}

export function update(props: {
  updateProcesses: UpdateProcesses,
}): UpdateProcesses {
  return props.updateProcesses;
}
