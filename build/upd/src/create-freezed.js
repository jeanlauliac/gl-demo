/* @flow */

'use strict';

// Create an freezed object that has the specified prototype and values.
export default function createFreezed<T>(prototype: Object, values: T): T {
  return Object.freeze(Object.assign(Object.create(prototype), values));
}
