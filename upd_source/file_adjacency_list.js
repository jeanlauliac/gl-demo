/* @flow */

'use strict';

import type {AdjacencyList} from './adjacency_list';
import type {FilePath} from './file_path';

export type FileAdjacencyList = AdjacencyList<FilePath, void>;
