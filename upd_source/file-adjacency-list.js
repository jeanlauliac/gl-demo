/* @flow */

'use strict';

import {AgentEvent} from './agent-event';

export type FileRelation = 'source' | 'dependency';
export type FileAdjacencyList = AdjacencyList<FilePath, FileRelation>;
