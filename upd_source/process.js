/* @flow */

'use strict';

import createFreezed from './create-freezed';
import * as immutable from 'immutable';

/**
 * Represent a process being run on the system.
 */
export type Process = {
  // Name of the binary to be run in the current context. Ex. 'ls'.
  command: string;
  // Ordered arguments, like on the command-line.
  args: immutable.List<string>;
};

const createRaw = createFreezed.bind(undefined, {
  valueOf(): Object {
    return immutable.List([this.command, this.args]);
  },
});

export function create(command: string, args: immutable.List<string>): Process {
  return createRaw({command, args});
}
