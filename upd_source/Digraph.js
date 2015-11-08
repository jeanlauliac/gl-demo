/* @flow */

'use strict'

import immutable from 'immutable'

class _VertexData<K> {
  succ: immutable._Set<K>;
  pred: immutable._Set<K>;
}

const EMPTY_VERTEX_DATA: any = {succ: immutable.Set(), pred: immutable.Set()}
function VertexData<K>(): _VertexData<K> {
  return EMPTY_VERTEX_DATA
}

/**
 * Immutable Directed Graph, with simple arcs and labeled vertices.
 */
class _Digraph<K> {

  _vertices: immutable._Map<K, _VertexData<K>>;
  order: number;

  constructor(vertices: immutable._Map<K, _VertexData<K>>) {
    this._vertices = vertices;
    // $FlowIssue: does not support accessors.
    Object.defineProperty(this, 'order', {get: function() {
      return this.count()
    }})
    Object.freeze(this);
  }

  count(): number {
    return this._vertices.count();
  }

  hashCode(): number {
    return this._vertices.hashCode();
  }

  equals(other: _Digraph<K>) {
    return other && this._vertices.equals(other._vertices);
  }

  /**
   * Add an isolated vertex. It does nothing if the vertex already exists.
   */
  add(vertex: K): _Digraph<K> {
    if (this._vertices.has(vertex)) {
      return this
    }
    return new _Digraph(this._vertices.set(vertex, VertexData()))
  }

  /**
   * Check if a vertex exists.
   */
  has(vertex: K): boolean {
    return this._vertices.has(vertex);
  }

  /**
   * Remove a vertex and all links from/to it.
   */
  remove(vertex: K): _Digraph<K> {
    if (!this.has(vertex)) {
      return this
    }
    let graph = this.successorsOf(vertex).reduce((graph, successor) => (
      graph.unlink(vertex, successor)
    ), this)
    graph = graph.predecessorsOf(vertex).reduce((graph, predecessor) => (
      graph.unlink(predecessor, vertex)
    ), graph)
    return new _Digraph(graph._vertices.remove(vertex))
  }

  /**
   * Add a new arc from one vertex to another. The vertices must already exist.
   */
  link(origin: K, target: K): _Digraph<K> {
    let vertices = this._vertices
    let originVertex = vertices.get(origin)
    if (originVertex == null) {
      return this
    }
    let targetVertex = vertices.get(target)
    if (targetVertex == null) {
      return this
    }
    originVertex = {succ: originVertex.succ.add(target), pred: originVertex.pred}
    targetVertex = {succ: targetVertex.succ, pred: targetVertex.pred.add(origin)}
    return new _Digraph(
      vertices.set(origin, originVertex).set(target, targetVertex)
    )
  }

  /**
   * Remove an arc from one vertex to another.
   */
  unlink(origin: K, target: K): _Digraph<K> {
    let vertices = this._vertices
    let originVertex = vertices.get(origin)
    if (originVertex == null) {
      return this
    }
    let targetVertex = vertices.get(target)
    if (targetVertex == null) {
      return this
    }
    originVertex = {succ: originVertex.succ.remove(target), pred: originVertex.pred}
    targetVertex = {succ: targetVertex.succ, pred: targetVertex.pred.remove(origin)}
    return new _Digraph(
      vertices.set(origin, originVertex).set(target, targetVertex)
    )
  }

  /**
   * Get all the successors of this element.
   */
  successorsOf(key: K): immutable._Set<K> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Set()
    }
    return vertexData.succ
  }

  /**
   * Get all the predecessors of this element.
   */
  predecessorsOf(key: K): immutable._Set<K> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Set()
    }
    return vertexData.pred
  }

}

export default function Digraph<K>(): _Digraph {
  return new _Digraph(immutable.Map())
}

Digraph._Digraph = _Digraph
