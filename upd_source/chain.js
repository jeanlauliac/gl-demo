/* @flow */

'use strict';

export default function chain<T, U>(value: T, fun: (value: T) => U): U {
  return fun(value);
}
