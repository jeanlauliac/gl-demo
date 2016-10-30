/* @flow */

'use strict';

import createFreezed from './create-freezed';
import * as immutable from 'immutable';
import * as path from 'path';

class AbstractFilePath {}

/**
 * Keeps track of the resolved path of a file. We should always use resolved
 * path for safety.
 */
export type FilePath = string & AbstractFilePath;

export function create(
  anyPath: string,
): FilePath {
  return (path.resolve(anyPath): any);
}

export function dirname(fp: FilePath): FilePath {
  return create(path.dirname(fp));
}
