/* @flow */

'use strict';

/**
 * Reduce a value through a series of functions.
 */
export default function chain<T>(funs: Array<(value: T) => T>, value: T): T {
  // $FlowIssue
  return funs.reduce((value, fun) => fun(value), value);
}
