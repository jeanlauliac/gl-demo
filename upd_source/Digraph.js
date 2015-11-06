/* @flow */

'use strict'

import immutable from 'immutable'

class _Node<K> {
  succ: immutable._Set<K>;
  pred: immutable._Set<K>;
}

const EMPTY_NODE = {succ: immutable.Set(), pred: immutable.Set()}
function Node(): _Node {
  return EMPTY_NODE
}

/**
 * Immutable Directed Graph.
 */
class _Digraph<K> {

  _nodes: immutable._Map<K, _Node<K>>;
  size: number;

  constructor(nodes: immutable._Map<K, _Node<K>>) {
    this._nodes = nodes;
    Object.defineProperty(this, 'size', {get: function() {
      return this.count()
    }})
    Object.freeze(this);
  }

  count(): number {
    return this._nodes.count();
  }

  hashCode(): number {
    return this._nodes.hashCode();
  }

  equals(other: _Digraph<K>) {
    return this._nodes.equals(other._nodes);
  }

  /**
   * Add an isolated vertex. It does nothing if the vertex already exists.
   */
  add(element: K): _Digraph<K> {
    if (this._nodes.has(element)) {
      return this
    }
    return new _Digraph(this._nodes.set(element, Node()))
  }

  /**
   * Adds a new edge from one element to another.
   */
  link(origin: K, target: K): _Digraph<K> {
    let nodes = this._nodes
    let originNode = nodes.get(origin) || Node()
    originNode = {succ: originNode.succ.add(target), pred: originNode.pred}
    let targetNode = nodes.get(target) || Node()
    targetNode = {succ: targetNode.succ, pred: targetNode.pred.add(origin)}
    return new _Digraph(nodes.set(origin, originNode).set(target, targetNode))
  }

  /**
   * Removes an edge from one element to another.
   */
  unlink(origin: K, target: K): _Digraph<K> {
    // FIXME
  }

  /**
   * Get all the successors of this element.
   */
  successorsOf(key: K): immutable._Set<K> {
    const node = this._nodes.get(key)
    if (node == null) {
      return immutable.Set()
    }
    return node.succ
  }

  /**
   * Get all the predecessors of this element.
   */
  predecessorsOf(key: K): immutable._Set<K> {
    const node = this._nodes.get(key)
    if (node == null) {
      return immutable.Set()
    }
    return node.pred
  }

}

export default function Digraph(): _Digraph {
  return new _Digraph(immutable.Map())
}

Digraph._Digraph = _Digraph
