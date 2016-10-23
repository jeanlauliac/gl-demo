/* @flow */

'use strict';

import createFreezed from './create-freezed';
import * as immutable from 'immutable';
import * as path from 'path';

/**
 * Keeps track of the resolved path of a file. We should always use resolved
 * path for safety.
 */
export type FilePath = {
  resolved: string,
}

const createRaw = createFreezed.bind(undefined, {
  toString(): string {
    return path.relative('.', this.resolved);
  },
  valueOf(): Object {
    return this.resolved;
  },
});

export function create(
  anyPath: string,
): FilePath {
  return createRaw({resolved: path.resolve(anyPath)});
}

export function dirname(fp: FilePath): FilePath {
  return create(path.dirname(fp.resolved));
}

export function relative(base: string, fp: FilePath): string {
  return path.relative(base, fp.resolved);
}
