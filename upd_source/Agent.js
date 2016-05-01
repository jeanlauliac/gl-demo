/* @flow */

'use strict';

import type {Spawn} from './ProcessAgent';
import type {FileAdjacencyList, FilePath} from './file-adjacency-list';
import type {AgentEvent} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';
import type {ImmList, ImmMap} from 'immutable';

import ProcessAgent from './ProcessAgent';
import {getProcesses, initialize, update} from './agent-state';
import immutable from 'immutable';
import util from 'util';

/**
 * Manage the state of the build at any point in time.
 */
export default class Agent {

  config: AgentConfig;
  state: AgentState;
  processAgent: ProcessAgent<string>;

  verboseLog(...args: Array<any>): void {
    if (this.config.verbose) {
      console.log('[verbose] %s', util.format(...args));
    }
  }

  _reconcile(): void {
    this.processAgent.update(getProcesses(this.config, this.state));
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: AgentEvent): void {
    this.verboseLog('Event `%s`', event.type);
    this.state = update(this.config, this.state, event);
    this._reconcile();
  }

  _onProcessExit(key: string, code: number, signal: string): void {
    this.update({code, key, signal, type: 'process_exit'});
  }

  constructor(config: AgentConfig, spawn: Spawn) {
    Object.defineProperty(this, 'config', {value: config});
    this.processAgent = new ProcessAgent(spawn);
    this.processAgent.on('exit', this._onProcessExit.bind(this));
    this.state = initialize(config);
    this._reconcile();
  }

}
