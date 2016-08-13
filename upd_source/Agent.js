/* @flow */

'use strict';

import type {Spawn} from './ProcessAgent';
import type {FileAdjacencyList, FilePath} from './file-adjacency-list';
import type {Event} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';
import type {ImmList, ImmMap} from 'immutable';

import ProcessAgent from './ProcessAgent';
import {getProcesses, initialize, update} from './agent-state';
import immutable from 'immutable';
import util from 'util';

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

/**
 * Manage the state of the build at any point in time.
 */
export default class Agent {

  config: AgentConfig;
  state: AgentState;
  processAgent: ProcessAgent<string>;
  _innerSpawn: Spawn;

  log(...args: Array<any>): void {
    const line = util.format(...args);
    const {columns} = process.stdout;
    if (columns == null || line.length < columns) {
      return void console.log(line);
    }
    console.log(line.substr(0, columns - 4) + '...');
  }

  verboseLog(...args: Array<any>): void {
    if (this.config.cliOpts.verbose) {
      this.log('[verbose] %s', util.format(...args));
    }
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: Event): void {
    this.verboseLog('event: %s', event.type);
    this.state = update(this.state, this.config, event, this.update);
  }

  _onProcessExit(key: string, code: number, signal: string): void {
    this.update({code, key, signal, type: 'process_exit'});
  }

  _spawn(command: string, args: Array<string>): child_process$ChildProcess {
    const proc = this._innerSpawn(command, args)
    const escapedArgs = args.map(escapeShellArg);
    this.verboseLog(`process/spawn(${proc.pid}): ` +
      `${command} ${escapedArgs.join(' ')}`);
    proc.on('exit', (code, signal) => {
      this.verboseLog(`process/exited(${proc.pid}): ${code} ${signal}`);
    });
    return proc;
  }

  constructor(config: AgentConfig, spawn: Spawn) {
    Object.defineProperty(this, 'config', {value: config});
    this._innerSpawn = spawn;
    this.processAgent = new ProcessAgent(this._spawn.bind(this));
    this.processAgent.on('exit', this._onProcessExit.bind(this));
    this.update = this.update.bind(this);
    this.state = initialize(config, this.update);
    this.verboseLog('initialized');
  }

}
