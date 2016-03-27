/* @flow */

'use strict';

import {Status} from './file-status';
import {UpdEvent} from './event';

export type UpdateResult = 'failure' | 'success';

export type Relation = 'source' | 'dependency';
export type Graph = Digraph<string, Status, Relation>;

/**
 * Update the graph when a file just got updated or the graph was filled.
 */
export function update(graph: Graph, event: Event): Graph {
  switch (event.type) {
    case 'file-updated':

  }
}
