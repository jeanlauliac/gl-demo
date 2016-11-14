/* @flow */

'use strict';

import type {Event} from './agent-event';
import type {AgentConfig, FileSet} from './agent-state';
import type {FilePath} from './file_path';

import {descendantsOf} from './file_adjacency_list';
import * as immutable from 'immutable';

export function create(): FileSet {
  return immutable.Set();
}

function updateForEvent(props: {
  config: AgentConfig,
  event: Event,
  scheduledFiles: FileSet,
  staleFiles: FileSet,
}): FileSet {
  const {scheduledFiles, event} = props;
  switch (event.type) {
    case 'update-requested':
      return scheduledFiles.union(props.staleFiles);
    case 'update-process-exit':
      const nextScheduledFiles = scheduledFiles.delete(event.targetPath);
      if (event.code === 0) {
        return nextScheduledFiles;
      }
      return nextScheduledFiles.subtract(descendantsOf(
        props.config.fileAdjacencyList,
        event.targetPath,
      ));
  }
  return scheduledFiles;
}

export function update(props: {
  config: AgentConfig,
  event: Event,
  scheduledFiles: FileSet,
  staleFiles: FileSet,
}): FileSet {
  return updateForEvent(props);
}
