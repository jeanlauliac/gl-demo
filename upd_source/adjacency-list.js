/* @flow */

'use strict';

import type {ImmKeyedIterable, ImmMap, ImmSet} from 'immutable';

import createFreezed from './create-freezed';
import nullthrows from './nullthrows';
import immutable from 'immutable';

// Describe the keys adjacent to a particular key.
type Adjacency<TKey, TValue> = {
  // All predecessors' keys.
  predecessors: ImmSet<TKey>;
  // Associate each of the successors' keys to the value of the arc that goes
  // from this key to a particular successor.
  successors: ImmMap<TKey, TValue>;
};

// Create a freezed adjacency object.
const adjacency = createFreezed.bind(undefined, {
  valueOf(): Object {
    return this.successors;
  },
  toString(): string {
    return this.successors.toString();
  },
});

// Representation of a directed adjacency list of keys. Two adjacent keys are
// linked by an arbitrary value (that can be `void`).
export type AdjacencyList<TKey, TValue> = ImmMap<TKey, Adjacency<TKey, TValue>>;

// Get a list that contains no adjacencies.
export function empty<TKey, TValue>(): AdjacencyList<TKey, TValue> {
  return immutable.Map();
}

// Get the total number of arcs.
export function count<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
): number {
  return list.reduce((total, adjacency) => {
    return adjacency.successors.size + total;
  }, 0);
}

// Check whether the list is empty.
export function isEmpty<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
): boolean {
  return count(list) === 0;
}

// Get an iterator over all the arcs of the list and their values.
export function arcs<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
): Iterator<[[TKey, TKey], TValue]> {
  return list.toSeq().flatMap((adjacency, originKey) => {
    return adjacency.successors.toSeq().mapKeys(targetKey => {
      return [originKey, targetKey];
    });
  }).entries();
}

// Get an iterable of all the arcs of the list and their values.
export function arcSeq<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
): ImmKeyedIterable<[TKey, TKey], TValue> {
  return immutable.Iterable.Keyed(arcs(list));
}

const EMPTY_ADJACENCY = {
  predecessors: immutable.Set(),
  successors: immutable.Map(),
};

// Link two keys with a valued arc. The value of the arc is just updated if the
// link already exists.
export function add<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  originKey: TKey,
  targetKey: TKey,
  value: TValue,
): AdjacencyList<TKey, TValue> {
  const origin = nullthrows(list.get(originKey, EMPTY_ADJACENCY));
  const target = nullthrows(list.get(targetKey, EMPTY_ADJACENCY));
  return list.withMutations(mlist => {
    mlist.set(originKey, adjacency({
      predecessors: origin.predecessors,
      successors: origin.successors.set(targetKey, value),
    }));
    mlist.set(targetKey, adjacency({
      predecessors: target.predecessors.add(originKey),
      successors: target.successors,
    }));
  });
}

export const set = add;

function isAdjacencyEmpty<TKey, TValue>(ajd: Adjacency<TKey, TValue>): boolean {
  return ajd.predecessors.isEmpty() && ajd.successors.isEmpty();
}

// Remove the link between two keys, if it exists. Return the same list
// otherwise.
export function remove<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  originKey: TKey,
  targetKey: TKey,
): AdjacencyList<TKey, TValue> {
  const origin = nullthrows(list.get(originKey, EMPTY_ADJACENCY));
  const target = nullthrows(list.get(targetKey, EMPTY_ADJACENCY));
  const nextOrigin = adjacency({
    predecessors: origin.predecessors,
    successors: origin.successors.remove(targetKey),
  });
  const nextList = isAdjacencyEmpty(nextOrigin) ?
    list.remove(originKey) : list.set(originKey, nextOrigin);
  const nextTarget = adjacency({
    predecessors: target.predecessors.remove(originKey),
    successors: target.successors,
  });
  return isAdjacencyEmpty(nextTarget) ?
    nextList.remove(targetKey) : nextList.set(targetKey, nextTarget);
}

// Check whether there exists an arc between two keys.
export function has<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  originKey: TKey,
  targetKey: TKey,
): boolean {
  const origin = nullthrows(list.get(originKey, EMPTY_ADJACENCY));
  return origin.successors.has(targetKey);
}

// Get the value of the arc between the two specifed keys. Return `undefined`
// or the specified default value if the arc does not exist.
export function get<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  originKey: TKey,
  targetKey: TKey,
  defValue?: TValue,
): TValue | void {
  const origin = nullthrows(list.get(originKey, EMPTY_ADJACENCY));
  if (origin.successors.has(targetKey)) {
    return defValue;
  }
  return origin.successors.get(targetKey);
}

// Get an iterator on the arcs going out of the specified key.
export function following<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  key: TKey,
): Iterator<[TKey, TValue]> {
  return nullthrows(list.get(key, EMPTY_ADJACENCY)).successors.entries();
}

// Get an iterable of the arcs going out of the specified key.
export function followingSeq<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  key: TKey,
): ImmKeyedIterable<TKey, TValue> {
  return immutable.Iterable.Keyed(following(list, key));
}

// Get an iterator on the arc going onto the specified key.
export function preceding<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  key: TKey,
): Iterator<[TKey, TValue]> {
  return nullthrows(list.get(key, EMPTY_ADJACENCY))
    .predecessors.toSeq().map(originKey => {
      const origin = nullthrows(list.get(originKey, EMPTY_ADJACENCY));
      return nullthrows(origin.successors.get(key));
    }).entries();
}

// Get an iterable of the arcs going onto the specified key.
export function precedingSeq<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
  key: TKey,
): ImmKeyedIterable<TKey, TValue> {
  return immutable.Iterable.Keyed(preceding(list, key));
}

// Return a string representation of the adjacency list.
export function toString<TKey, TValue>(
  list: AdjacencyList<TKey, TValue>,
): string {
  const arcStrings = arcSeq(list).map((value, [originKey, targetKey]) => {
    return `${originKey}=>${targetKey}: ${value}`;
  });
  return `adjacency-list {${arcStrings.join(', ')}}`;
}
