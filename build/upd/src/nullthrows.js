/* @flow */

'use strict';

export default function nullthrows<V>(value: ?V): V {
  if (value == null) {
    throw new Error('value cannot be null or undefined');
  }
  return value;
}
