/* @flow */

'use strict'

import immutable from 'immutable'

class LightweightImmutable {

  _immutableData: {fieldMap: ?immutable._Map<string, mixed>};

  constructor(fields: Object) {
    Object.defineProperty(this, '_immutableData', {value: {fieldMap: null}})
    Object.assign(this, fields)
    Object.freeze(this)
  }

  valueOf(): immutable._Map<string, mixed> {
    let fieldMap = this._immutableData.fieldMap
    if (fieldMap == null) {
      fieldMap = this._immutableData.fieldMap = immutable.Map(this)
    }
    return fieldMap
  }

}

class Vertex<K, V> extends LightweightImmutable {

  succ: immutable._Set<K>;
  pred: immutable._Set<K>;
  value: V;

  static of(value: V): Vertex<K, V> {
    return new Vertex({succ: immutable.Set(), pred: immutable.Set(), value});
  }

  setValue(value: V): Vertex<K, V> {
    return new Vertex({succ: this.succ, pred: this.pred, })
  }

  removeAllLinksTo(key: K): Vertex<K, V> {
    const succ = this.succ.remove(key)
    const pred = this.pred.remove(key)
    if (succ === this.succ && pred === this.pred) {
      return this
    }
    return new Vertex({succ, pred, value: this.value})
  }

  linkTo(key: K): Vertex<K, V> {
    return new Vertex(
      {succ: this.succ.add(key), pred: this.pred, value: this.value}
    )
  }

  linkFrom(key: K): Vertex<K, V> {
    return new Vertex(
      {succ: this.succ, pred: this.pred.add(key), value: this.value}
    )
  }

  unlinkTo(key: K): Vertex<K, V> {
    return new Vertex(
      {succ: this.succ.remove(key), pred: this.pred, value: this.value}
    )
  }

  unlinkFrom(key: K): Vertex<K, V> {
    return new Vertex(
      {succ: this.succ, pred: this.pred.remove(key), value: this.value}
    )
  }

}

let emptyDigraph: any = null;

/**
 * Immutable Directed Graph, with simple arcs and valued vertices.
 */
export default class Digraph<K, V> extends LightweightImmutable {

  _vertices: immutable._Map<K, Vertex<K, V>>;

  static empty(): Digraph<K, V> {
    if (emptyDigraph == null) {
      emptyDigraph = new Digraph({_vertices: immutable.Map()})
    }
    return emptyDigraph
  }

  order(): number {
    return this._vertices.count()
  }

  /**
   * Set a vertex value by its key.
   */
  set(key: K, value: V): Digraph<K, V> {
    const vertex = this._vertices.get(key);
    return new Digraph({_vertices: this._vertices.set(key, (
      vertex != null ? vertex.setValue(value) : Vertex.of(value)
    ))})
  }

  get(key: K): ?V {
    const vertex = this._vertices.get(key)
    if (vertex == null) {
      return undefined
    }
    return vertex.value
  }

  /**
   * Check if a vertex exists by its key.
   */
  has(key: K): boolean {
    return this._vertices.has(key)
  }

  /**
   * Remove a vertex and all links from/to it.
   */
  remove(key: K): Digraph<K, V> {
    if (!this._vertices.has(key)) {
      return this
    }
    const vertices = this._vertices.map(vertex => vertex.removeAllLinksTo(key))
    return new Digraph({_vertices: vertices.remove(key)})
  }

  /**
   * Add a new arc from one vertex to another. The vertices must already exist.
   */
  link(origin: K, target: K): Digraph<K, V> {
    let vertices = this._vertices
    let originVertex = vertices.get(origin)
    if (originVertex == null) {
      return this
    }
    let targetVertex = vertices.get(target)
    if (targetVertex == null) {
      return this
    }
    return new Digraph({_vertices: vertices
      .set(origin, originVertex.linkTo(target))
      .set(target, targetVertex.linkFrom(origin))
    })
  }

  /**
   * Remove an arc from one vertex to another.
   */
  unlink(origin: K, target: K): Digraph<K, V> {
    let vertices = this._vertices
    let originVertex = vertices.get(origin)
    if (originVertex == null) {
      return this
    }
    let targetVertex = vertices.get(target)
    if (targetVertex == null) {
      return this
    }
    return new Digraph({_vertices: vertices
      .set(origin, originVertex.unlinkTo(target))
      .set(target, targetVertex.unlinkFrom(origin))
    })
  }

  /**
   * Get all the successors of this element.
   */
  following(key: K): immutable._Set<K> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Set()
    }
    return vertexData.succ
  }

  /**
   * Get all the predecessors of this element.
   */
  preceding(key: K): immutable._Set<K> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Set()
    }
    return vertexData.pred
  }

}
