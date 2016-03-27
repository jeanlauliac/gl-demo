/* @flow */

'use strict';

import {add, count, empty, followingSeq, has, isEmpty,
  precedingSeq, remove, toString} from './adjacency-list';
import {is} from 'immutable'
import tap from 'tap';

tap.test('adjacency-list', t => {
  const emptyList = empty();
  t.equal(count(emptyList), 0, 'has no arcs');
  t.equal(isEmpty(emptyList), true, 'is empty');

  const someArcs = (
    [[1, 2, 'foo'], [2, 3, 'bar'], [1, 3, 'glo']]
      .reduce((list, args) => add(list, ...args), emptyList)
  );
  t.similar(count(someArcs), 3, 'has correct count');
  t.similar(has(someArcs, 1, 2), true, 'has arc');
  t.similar(has(someArcs, 3, 1), false, 'has not arc');
  t.similar(followingSeq(someArcs, 1).entrySeq().toArray(),
    [[2, 'foo'], [3, 'glo']], 'has correct successors');
  t.similar(followingSeq(someArcs, 2).entrySeq().toArray(),
    [[3, 'bar']], 'has correct successors');
  t.similar(followingSeq(someArcs, 3).entrySeq().toArray(),
    [], 'has correct successors');
  t.similar(precedingSeq(someArcs, 1).entrySeq().toArray(),
    [], 'has correct predecessors');
  t.similar(precedingSeq(someArcs, 2).entrySeq().toArray(),
    [[1, 'foo']], 'has correct predecessors');
  t.similar(precedingSeq(someArcs, 3).entrySeq().toArray(),
    [[2, 'bar'], [1, 'glo']], 'has correct predecessors');

  const lessArcs = remove(someArcs, 1, 2);
  t.similar(followingSeq(lessArcs, 1).entrySeq().toArray(),
    [[3, 'glo']], 'has correct successors');
  t.similar(precedingSeq(someArcs, 3).entrySeq().toArray(),
    [[2, 'bar'], [1, 'glo']], 'has correct predecessors');

  t.equal(is(someArcs, add(lessArcs, 1, 2, 'foo')),
    true, 'implements value semantics');
  t.equal(is(someArcs, add(lessArcs, 1, 2, 'beep')),
    false, 'implements value semantics');

  t.equal(toString(someArcs),
    "adjacency-list {1=>2: foo, 1=>3: glo, 2=>3: bar}",
    'implements toString');

  t.end();
});
