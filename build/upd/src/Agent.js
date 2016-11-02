/* @flow */

'use strict';

import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';
import type {Event} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';

import * as agentState from './agent-state';
import * as file_path from './file_path';
import chokidar from 'chokidar';
import fs from 'fs';
import path from 'path';
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

  setPrompt(prompt: ?string): ?string {
    if (prompt === this._currentPrompt) {
      return prompt;
    }
    if (this._currentPrompt != null && process.stdout.isTTY) {
      readline.moveCursor(process.stdout, 0, -1);
      readline.clearLine(process.stdout, 0);
    }
    if (prompt != null) {
      console.log(prompt);
    }
    const oldPrompt = this._currentPrompt;
    this._currentPrompt = prompt;
    return oldPrompt;
  }

  clearPrompt(): ?string {
    return this.setPrompt(null);
  }

  log(...args: Array<any>): void {
    const prompt = this.clearPrompt();
    let line = util.format(...args);
    // $FlowIssue: missing decl for `columns`.
    const {columns} = process.stdout;
    if (columns != null && line.length >= columns) {
      line = line.substr(0, columns - 4) + '...';
    }
    console.log(line);
    this.setPrompt(prompt);
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

  _updatePrompt() {
    const totalCount = this.config.fileBuilders.size;
    const updatedCount = totalCount - this.state.staleFiles.size;
    const percent = Math.ceil((updatedCount / totalCount) * 100);
    const finish = totalCount === updatedCount ? ', done.' : '...';
    this.setPrompt(
      `☕  Updating files: ${percent}% (${updatedCount}/${totalCount})${finish}`,
    );
  }

  _logProcessOutput(event: Event): void {
    if (event.type !== 'update-process-exit') {
      return;
    }
    if (event.stdout.length === 0 && event.stderr.length === 0) {
      return;
    }
    const prompt = this.clearPrompt();
    process.stdout.write(event.stdout);
    process.stderr.write(event.stderr);
    this.setPrompt(prompt);
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: Event): void {
    this.verboseLog('agent/event: %s', event.type);
    this._logProcessOutput(event);
    this.state = agentState.update({
      config: this.config,
      createDirectory: this._createDirectory,
      dispatch: this.update,
      event,
      spawn: this._spawn,
      state: this.state,
    });
    this._updatePrompt();
  }

  _onExit(): void {
    const {state} = this;
    if (!state.staleFiles.isEmpty()) {
      this.log(this.clearPrompt());
      this.log('*** Update failed! Check error messages in output.');
      // https://nodejs.org/api/process.html#process_exit_codes
      process.exitCode = 20;
    }
  }

  _fsWatcher: any;

  _watchFilesytem() {
    this._fsWatcher = chokidar.watch('.', {
      ignored: ['.*/**', '**/node_modules/**'],
      ignoreInitial: true,
    });
    this._fsWatcher.on('add', filePath => {
      this.verboseLog('watcher/add: %s', path.relative('.', filePath));
    });
    this._fsWatcher.on('change', filePath => {
      this.verboseLog('watcher/change: %s', path.relative('.', filePath));
      this.update({
        filePath: file_path.create(filePath),
        type: 'chokidar-file-changed',
      });
    });
    this._fsWatcher.on('unlink', filePath => {
      this.verboseLog('watcher/unlink: %s', path.relative('.', filePath));
    });
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
    this._watchFilesytem();
  }

}
