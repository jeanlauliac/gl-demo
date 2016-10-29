/* @flow */

'use strict';

import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';
import type {Event} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';

import * as agentState from './agent-state';
import fs from 'fs';
import {List as ImmList, Map as ImmMap} from 'immutable';
import * as readline from 'readline';
import util from 'util';

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

export type Spawn = (command: string, args: Array<string>) =>
  child_process$ChildProcess;

/**
 * Manage the state of the build at any point in time.
 */
export default class Agent {

  config: AgentConfig;
  state: AgentState;
  _innerSpawn: Spawn;
  _currentPrompt: ?string;

  log(...args: Array<any>): void {
    this._currentPrompt = null;
    const line = util.format(...args);
    // $FlowIssue: missing decl for `columns`.
    const {columns} = process.stdout;
    if (columns == null || line.length < columns) {
      console.log(line);
      return;
    }
    console.log(line.substr(0, columns - 4) + '...');
  }

  setPrompt(...promptArgs: Array<any>): void {
    const prompt = util.format(...promptArgs);
    if (prompt === this._currentPrompt) {
      return;
    }
    if (process.stdout.isTTY && this._currentPrompt != null) {
      readline.moveCursor(process.stdout, 0, -1);
    }
    console.log(prompt);
    this._currentPrompt = prompt;
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

  _spawn(command: string, args: Array<string>): child_process$ChildProcess {
    const proc = this._innerSpawn(command, args)
    const escapedArgs = args.map(escapeShellArg);
    this.verboseLog(`process/spawn: [${proc.pid}] ` +
      `${command} ${escapedArgs.join(' ')}`);
    proc.on('exit', (code, signal) => {
      this.verboseLog(`process/exited: [${proc.pid}] ${code} ${signal}`);
    });
    return proc;
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: Event): void {
    this.verboseLog('agent/event: %s', event.type);
    this.state = agentState.update({
      config: this.config,
      createDirectory: this._createDirectory,
      dispatch: this.update,
      event,
      spawn: this._spawn,
      state: this.state,
    });
    const totalCount = this.config.fileBuilders.size;
    const updatedCount = totalCount - this.state.staleFiles.size;
    this.setPrompt(`☕  Updating [${updatedCount}/${totalCount}]`);
  }

  _onExit(): void {
    const {state} = this;
    if (!state.staleFiles.isEmpty()) {
      this.log(`🎃  Update failed: ${state.staleFiles.size} files are stale.`);
      // $FlowIssue: missing declaration for modern `exitCode` prop.
      process.exitCode = 1;
    } else {
      this.log('✨  Done.');
    }
  }

  constructor(config: AgentConfig, spawn: Spawn) {
    Object.defineProperty(this, 'config', {value: config});
    this._currentPrompt = null;
    this._innerSpawn = spawn;
    (this: any).update = this.update.bind(this);
    (this: any)._createDirectory = this._createDirectory.bind(this);
    (this: any)._spawn = this._spawn.bind(this);
    this.verboseLog('agent/start');
    this.state = agentState.create({
      config,
      createDirectory: this._createDirectory,
      dispatch: this.update,
      spawn: this._spawn,
    });
    process.on('exit', this._onExit.bind(this));
  }

}
