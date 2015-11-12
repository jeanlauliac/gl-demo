/* @flow */

'use strict'

import Digraph from './Digraph'
import tap from 'tap'

tap.test('Digraph', t => {
  let graph = Digraph.empty()
  t.equal(graph.order(), 0, 'graph is empty')
  t.equal(graph.set(1).has(1), true, 'can add vertex')
  t.equal(graph.set(1, 'foo').get(1), 'foo', 'can add vertex with value')
  t.equal(graph.set(1).remove(1).has(1), false, 'can remove vertex')
  graph = graph.set(1).set(2).link(1, 2).set(3).link(2, 3).link(1, 3)
  t.similar(graph.following(1).toArray(), [2, 3], 'has correct successors')
  t.similar(graph.following(2).toArray(), [3], 'has correct successors')
  t.similar(graph.following(3).toArray(), [], 'has correct successors')
  t.similar(graph.preceding(1).toArray(), [], 'has correct predecessors')
  t.similar(graph.preceding(2).toArray(), [1], 'has correct predecessors')
  t.similar(graph.preceding(3).toArray(), [2, 1], 'has correct predecessors')
  graph = graph.remove(2)
  t.similar(graph.following(1).toArray(), [3], 'has correct successors')
  t.similar(graph.preceding(3).toArray(), [1], 'has correct predecessors')
  t.end()
})
