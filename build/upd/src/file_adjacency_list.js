/* @flow */

'use strict';

import type {AdjacencyList} from './adjacency_list';
import type {FilePath} from './file_path';

import * as adjacency_list from './adjacency_list';
import * as immutable from 'immutable';

export type FileAdjacencyList = AdjacencyList<FilePath, void>;

export function descendantsOf<TKey, TValue>(
  adjList: AdjacencyList<TKey, TValue>,
  originKey: TKey,
): immutable.Set<TKey> {
  return adjacency_list.followingSeq(adjList, originKey).keySeq().reduce(
    (descendants, followingKey) => {
      return descendants.union(
        [followingKey],
        descendantsOf(adjList, followingKey),
      );
    },
    immutable.Set(),
  );
}
