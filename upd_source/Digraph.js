/* @flow */

'use strict'

import LightweightImmutable from './LightweightImmutable'
import immutable from 'immutable'
import nullthrows from './nullthrows'

class Vertex<K, V> extends LightweightImmutable {

  succ: immutable._Set<K>;
  pred: immutable._Set<K>;
  value: V;

  static of(value: V): Vertex<K, V> {
    return new Vertex({succ: immutable.Set(), pred: immutable.Set(), value});
  }

  setValue(value: V): Vertex<K, V> {
    if (value === this.value) {
      return this
    }
    return new Vertex({succ: this.succ, pred: this.pred, value})
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
  order: number;

  constructor(vertices: immutable._Map<K, Vertex<K, V>>) {
    super({_vertices: vertices, order: vertices.size})
  }

  static empty(): Digraph<K, V> {
    if (emptyDigraph == null) {
      emptyDigraph = new Digraph(immutable.Map())
    }
    return emptyDigraph
  }

  /**
   * Set a vertex value by its key.
   */
  set(key: K, value: V): Digraph<K, V> {
    const vertex = this._vertices.get(key);
    return new Digraph(this._vertices.set(key, (
      vertex != null ? vertex.setValue(value) : Vertex.of(value)
    )))
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
    return new Digraph(vertices.remove(key))
  }

  _updateLinks(
    originKey: K,
    targetKey: K,
    updater: (origin: Vertex<K, V>, target: Vertex<K, V>) => {
      origin: Vertex<K, V>,
      target: Vertex<K, V>,
    }
  ): Digraph<K, V> {
    const vertices = this._vertices
    const origin = vertices.get(originKey)
    if (origin == null) {
      return this
    }
    const target = vertices.get(targetKey)
    if (target == null) {
      return this
    }
    const result = updater(origin, target)
    return new Digraph(vertices
      .set(originKey, result.origin)
      .set(targetKey, result.target)
    )
  }

  /**
   * Add a new arc from one vertex to another. The vertices must already exist.
   */
  link(originKey: K, targetKey: K): Digraph<K, V> {
    return this._updateLinks(originKey, targetKey, (origin, target) => (
      {origin: origin.linkTo(targetKey), target: target.linkFrom(originKey)}
    ))
  }

  /**
   * Remove an arc from one vertex to another.
   */
  unlink(originKey: K, targetKey: K): Digraph<K, V> {
    return this._updateLinks(originKey, targetKey, (origin, target) => (
      {origin: origin.unlinkTo(targetKey), target: target.unlinkFrom(originKey)}
    ))
  }

  /**
   * Get all the successors of this element.
   */
  following(key: K): immutable._Iterable_Keyed<K, V> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Iterable.Keyed()
    }
    return immutable.Iterable.Keyed(vertexData.succ.toSeq().map(
      key => ([key, nullthrows(this._vertices.get(key)).value])
    ))
  }

  /**
   * Get all the predecessors of this element.
   */
  preceding(key: K): immutable._Iterable_Keyed<K, V> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Iterable.Keyed()
    }
    return immutable.Iterable.Keyed(vertexData.pred.toSeq().map(
      key => ([key, nullthrows(this._vertices.get(key)).value])
    ))
  }

  some(pred: (value: V, key: K) => boolean): boolean {
    return this._vertices.some((vertex, key) => pred(vertex.value, key))
  }

  toSeq(): immutable._Iterable_Keyed<K, V> {
    return this._vertices.toSeq().map(vertex => vertex.value)
  }

  map(iter: (value: V, key: K) => V): Digraph<K, V> {
    return new Digraph(this._vertices.map((vertex, key) => (
      vertex.setValue(iter(vertex.value, key))
    )))
  }

  isEmpty(): boolean {
    return this.order === 0
  }

}
