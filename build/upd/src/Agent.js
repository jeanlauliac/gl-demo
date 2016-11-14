/* @flow */

'use strict';

import type {FileAdjacencyList} from './file_adjacency_list';
import type {FilePath} from './file_path';
import type {Event} from './agent-event';
import type {AgentConfig, AgentState} from './agent-state';

import Terminal from './Terminal';
import * as agentState from './agent-state';
import * as file_path from './file_path';
import chokidar from 'chokidar';
import dnode from 'dnode';
import fs from 'fs';
import path from 'path';
import {List as ImmList, Map as ImmMap} from 'immutable';
import util from 'util';

function escapeShellArg(arg) {
  return arg.replace(/( )/, '\\$1')
}

export type Spawn = (command: string, args: Array<string>) =>
  child_process$ChildProcess;

const terminal = Terminal.get();

/**
 * Manage the state of the build at any point in time.
 */
export default class Agent {

  config: AgentConfig;
  state: AgentState;
  _innerSpawn: Spawn;
  _statusListeners = [];
  _processStatuses = [];

  _createDirectory(directoryPath: FilePath): Promise<void> {
    return new Promise((resolve, reject) => {
      terminal.log('mkdir/call: %s', directoryPath);
      fs.mkdir(directoryPath, undefined, error => {
        if (error) {
          terminal.log('mkdir/reject: %s %s', directoryPath, error);
          return reject(error);
        }
        terminal.log('mkdir/resolve: %s', directoryPath);
        resolve();
      });
    });
  }

  _spawn(command: string, args: Array<string>): child_process$ChildProcess {
    const proc = this._innerSpawn(command, args)
    const escapedArgs = args.map(escapeShellArg);
    terminal.log(`process/spawn: [${proc.pid}] ` +
      `${command} ${escapedArgs.join(' ')}`);
    proc.on('exit', (code, signal) => {
      terminal.log(`process/exited: [${proc.pid}] ${code} ${signal}`);
    });
    return proc;
  }

  _flushStatusListeners() {
    this._statusListeners.forEach(listener => {
      listener(undefined, {
        staleFileCount: this.state.staleFiles.size,
        scheduledFileCount: this.state.scheduledFiles.size,
        processes: this._processStatuses,
      });
    });
    this._statusListeners = [];
    this._processStatuses = [];
  }

  _storeProcessOutput(event: Event): void {
    if (event.type !== 'update-process-exit') {
      return;
    }
    const {stdout, stderr} = event;
    this._processStatuses.push({
      desc: event.updateDesc,
      stdout: event.stdout.toString(),
      stderr: event.stderr.toString(),
      targetPath: event.targetPath,
    });
  }

  /**
   * Update the state based on a single event, and reconcile any process or
   * file that is described as running or open.
   */
  update(event: Event): void {
    terminal.log('agent/event: %s', event.type);
    this._storeProcessOutput(event);
    this.state = agentState.update({
      config: this.config,
      createDirectory: this._createDirectory,
      dispatch: this.update,
      event,
      spawn: this._spawn,
      state: this.state,
    });
    this._flushStatusListeners();
  }

  _fsWatcher: any;

  _watchFilesytem() {
    if (this.config.cliOpts.once) {
      return;
    }
    this._fsWatcher = chokidar.watch('.', {
      ignored: ['.*/**', '**/node_modules/**'],
      ignoreInitial: true,
    });
    this._fsWatcher.on('add', filePath => {
      terminal.log('watcher/add: %s', path.relative('.', filePath));
    });
    this._fsWatcher.on('change', filePath => {
      terminal.log('watcher/change: %s', path.relative('.', filePath));
      this.update({
        filePath: file_path.create(filePath),
        type: 'chokidar-file-changed',
      });
    });
    this._fsWatcher.on('unlink', filePath => {
      terminal.log('watcher/unlink: %s', path.relative('.', filePath));
    });
    const onExitRequested = () => {
      this._fsWatcher.close();
    };
    process.on('SIGINT', onExitRequested);
    process.on('SIGTERM', onExitRequested);
  }

  _server: net$Server;

  _startDNode() {
    this._server = dnode({
      update: callback => {
        this.update({type: 'update-requested'});
        this._statusListeners.push(callback);
        this._flushStatusListeners();
      },
      waitForStatus: callback => {
        this._statusListeners.push(callback);
      },
    }).listen(5004);
    const onExitRequested = () => {
      this._server.close();
    };
    process.on('SIGINT', onExitRequested);
    process.on('SIGTERM', onExitRequested);
  }

  constructor(config: AgentConfig, spawn: Spawn) {
    Object.defineProperty(this, 'config', {value: config});
    this._innerSpawn = spawn;
    (this: any).update = this.update.bind(this);
    (this: any)._createDirectory = this._createDirectory.bind(this);
    (this: any)._spawn = this._spawn.bind(this);
    terminal.log('agent/start');
    this.state = agentState.create({
      config,
      createDirectory: this._createDirectory,
      dispatch: this.update,
      spawn: this._spawn,
    });
    this._watchFilesytem();
    this._startDNode();
  }

}
