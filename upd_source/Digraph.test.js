/* @flow */

'use strict'

import Digraph from './Digraph'
import tap from 'tap'

tap.test('Digraph', t => {
  let graph = Digraph()
  t.equal(graph.count(), 0, 'graph is empty')
  t.equal(graph.order, 0, 'graph is empty')
  t.equal(graph.add(1).has(1), true, 'can add vertex')
  t.equal(graph.add(1).remove(1).has(1), false, 'can remove vertex')
  graph = graph.add(1).add(2).link(1, 2).add(3).link(2, 3).link(1, 3)
  t.similar(graph.successorsOf(1).toArray(), [2, 3], 'has correct successors')
  t.similar(graph.successorsOf(2).toArray(), [3], 'has correct successors')
  t.similar(graph.successorsOf(3).toArray(), [], 'has correct successors')
  t.similar(graph.predecessorsOf(1).toArray(), [], 'has correct predecessors')
  t.similar(graph.predecessorsOf(2).toArray(), [1], 'has correct predecessors')
  t.similar(graph.predecessorsOf(3).toArray(), [2, 1], 'has correct predecessors')
  graph = graph.remove(2)
  t.similar(graph.successorsOf(1).toArray(), [3], 'has correct successors')
  t.similar(graph.predecessorsOf(3).toArray(), [1], 'has correct predecessors')
  t.end()
})
