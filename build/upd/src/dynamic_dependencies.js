/* @flow */

'use strict';

import type {DispatchEvent, Event} from './agent-event';
import type {AgentConfig, FileSet} from './agent-state';
import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';

import * as adjacency_list from './adjacency_list';
import * as file_path from './file_path';
import * as fs from 'fs';
import * as immutable from 'immutable';
import * as path from 'path';

export type DynamicDependencies = FileAdjacencyList;

export function create(): DynamicDependencies {
  return adjacency_list.empty();
}

export function update(props: {
  config: AgentConfig,
  dispatch: DispatchEvent,
  dynamicDependencies: DynamicDependencies,
  event: Event,
}): DynamicDependencies {
  const {dynamicDependencies, event} = props;
  switch (event.type) {
    case 'update-process-exit':
      const depFilePath = event.updateDesc.dynamicDependenciesFilePath;
      if (event.code !== 0 || depFilePath == null) {
        break;
      }
      fs.readFile(depFilePath, 'utf8', (error, content) => {
        if (error) {
          return;
        }
        props.dispatch({
          content,
          type: 'dynamic-dependencies-file-read'
        });
      });
      break;
    case 'dynamic-dependencies-file-read':
      const {content} = event;
      // Ain't got no time for a true parser.
      const m = content.match(/([^:]*): ((?:.*\n)+)/);
      if (m == null) {
        throw new Error('unexpected invalid dynamic dependency file');
      }
      const targetPath = file_path.create(m[1]);
      const newDepNames =
        immutable.List(m[2].split(/[\\\n ]+/)).filter(f => f !== '');
      return newDepNames.reduce((adjList, newDepName) => {
        return adjacency_list.add(
          adjList,
          file_path.create(newDepName),
          targetPath,
        );
      }, dynamicDependencies);
  }
  return props.dynamicDependencies;
}
