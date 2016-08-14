/* @flow */

'use strict';

import type {Spawn} from './ProcessAgent';
import type {FileAdjacencyList, FilePath} from './file-adjacency-list';
import type {Event} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';

import ProcessAgent from './ProcessAgent';
import * as agentState from './agent-state';
import fs from 'fs';
import {List as ImmList, Map as ImmMap} from 'immutable';
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
  _innerSpawn: Spawn;

  log(...args: Array<any>): void {
    const line = util.format(...args);
    // $FlowIssue: missing decl for `columns`.
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

  _createDirectory(directoryPath: FilePath): Promise<void> {
    return new Promise((resolve, reject) => {
      this.verboseLog('mkdir/call: %s', directoryPath);
      fs.mkdir(directoryPath, undefined, error => {
        if (error) {
          this.verboseLog('mkdir/reject: %s %s', directoryPath, error);
          return reject(error);
        }
        this.verboseLog('mkdir/resolve: %s', directoryPath);
        resolve();
      });
    });
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: Event): void {
    this.verboseLog('event: %s', event.type);
    this.state = agentState.update(
      this.state,
      this.config,
      event,
      this.update,
      this._createDirectory,
    );
  }

  _onProcessExit(key: string, code: number, signal: string): void {
    this.update({code, key, signal, type: 'process-exit'});
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
    (this: any).update = this.update.bind(this);
    (this: any)._createDirectory = this._createDirectory.bind(this);
    this.state = agentState.create(config, this.update, this._createDirectory);
    this.verboseLog('initialized');
  }

}
