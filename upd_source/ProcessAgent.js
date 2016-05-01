/* @flow */

'use strict';

import type {Process} from './process';
import type {ImmMap} from 'immutable';

import chain from './chain';
import nullthrows from './nullthrows';
import {EventEmitter} from 'events';
import immutable from 'immutable';

export type Spawn = (command: string, args: Array<string>) =>
  child_process$ChildProcess;

/**
 * Maintain processes running in the host system based a keyed collection of
 * process descriptors.
 */
export default class ProcessAgent<K> extends EventEmitter {

  insts: ImmMap<K, {process: Process, instance: child_process$ChildProcess}>;
  spawn: Spawn;

  constructor(spawn: Spawn) {
    super();
    this.insts = immutable.Map();
    this.spawn = spawn;
  }

  _onExit(key: K, code: number, signal: string): void {
    this.insts = this.insts.delete(key);
    this.emit('exit', key, code, signal);
  }

  /**
   * Compare the specified list of processes to the process we know are
   * currently running. Create processes for which the key is new, and kill
   * processes for which the key has been removed from the map.
   */
  update(processes: ImmMap<K, Process>): void {
    const procKeys = immutable.Set(processes.keys());
    const instKeys = immutable.Set(this.insts.keys());
    this.insts = chain([
      insts => procKeys.subtract(instKeys).reduce((insts, newKey) => {
        const process = nullthrows(processes.get(newKey));
        const instance = this.spawn(process.command, process.args.toArray());
        instance.on('exit', this._onExit.bind(this, newKey));
        return insts.set(newKey, {instance, process})
      }, insts),
      insts => instKeys.subtract(procKeys).reduce((insts, removedKey) => {
        const inst = nullthrows(insts.get(removedKey));
        inst.instance.kill();
        return insts.delete(removedKey);
      }, insts),
    ], this.insts);
  }

}
