/* @flow */

'use strict';

import {AdjacencyList} from './adjacency-list';
import {AgentEvent} from './agent-event';

export type FilePath = string;
export type FileAdjacencyList = AdjacencyList<FilePath, void>;
