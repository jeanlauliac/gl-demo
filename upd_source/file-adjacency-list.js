/* @flow */

'use strict';

import type {AdjacencyList} from './adjacency-list';

export type FilePath = string;
export type FileAdjacencyList = AdjacencyList<FilePath, void>;
