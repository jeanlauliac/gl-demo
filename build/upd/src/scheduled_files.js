/* @flow */

'use strict';

import type {Event} from './agent-event';
import type {AgentConfig, FileSet} from './agent-state';
import type {FilePath} from './file_path';

import * as immutable from 'immutable';

export function create(): FileSet {
  return immutable.Set();
}

function updateForEvent(props: {
  event: Event,
  scheduledFiles: FileSet,
  staleFiles: FileSet,
}): FileSet {
  const {scheduledFiles} = props;
  switch (props.event.type) {
    case 'update-requested':
      return scheduledFiles.union(props.staleFiles);
  }
  return scheduledFiles;
}

export function update(props: {
  event: Event,
  scheduledFiles: FileSet,
  staleFiles: FileSet,
}): FileSet {
  const scheduledFiles = updateForEvent(props);
  return scheduledFiles.intersect(props.staleFiles);
}
