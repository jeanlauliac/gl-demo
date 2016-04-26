/* @flow */

'use strict';

import type {spawn} from 'child_process';

import immutable from 'immutable';

type Spawn = typeof process.spawn;

/**
 * Maintain processes in the system based on an algebraic list of processes.
 */
export default class ProcessAgent {

  insts: Array<{
    process: Process,
    instance: Object,
  }>;
  spawn: Spawn;

  constructor(spawn: Spawn) {
    this.insts = [];
    this.spawn = spawn;
  }

  /**
   * Compare the specified list of processes to the process we know are
   * currently running. Create processes that are not running yet, and kill
   * processes that have been removed from the list.
   */
  update(processes: ImmList<Process>) {
    let instIx = 0;
    let procIx = 0;
    while (instIx < this.insts.length) {
      const inst = this.insts[instIx];
      if (
        procIx < processes.size &&
        immutable.is(inst.process, processes.get(procIx))
      ) {
        ++procIx;
        ++instIx;
        continue;
      }
      inst.instance.kill();
      this.insts.splice(instIx, 1);
    }
    while (procIx < processes.size) {
      const proc = processes.get(procIx);
      const instance = this.spawn(proc.command, proc.args);
      this.insts.push({instance, process: proc});
      ++procIx;
    }
  }

}
