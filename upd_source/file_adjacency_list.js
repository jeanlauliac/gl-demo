/* @flow */

'use strict';

import type {AdjacencyList} from './adjacency_list';

export type FilePath = string;
export type FileAdjacencyList = AdjacencyList<FilePath, void>;
