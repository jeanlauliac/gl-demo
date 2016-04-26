/* @flow */

'use strict';

import type {ImmList, ImmMap, ImmSet} from 'immutable';

import createFreezed from './create-freezed';
import immutable from 'immutable';

type Process = {
  // Process to run.
  command: string;
  // Ordered arguments, like on the command-line.
  args: ImmList<string>;
};

const createRaw = createFreezed.bind(undefined, {
  valueOf(): Object {
    return immutable.List([this.command, this.args]);
  },
});

export function create(command: string, args: ImmList<string>): Process {
  return createRaw({command, args});
}
