/* @flow */

'use strict'

import LightweightImmutable from './LightweightImmutable'
import immutable from 'immutable'
import nullthrows from './nullthrows'

class Vertex<K, V, L> extends LightweightImmutable {

  succ: immutable._Map<K, L>;
  pred: immutable._Set<K>;
  value: V;

  static of(value: V): Vertex<K, V, L> {
    return new Vertex({succ: immutable.Map(), pred: immutable.Set(), value});
  }

  setValue(value: V): Vertex<K, V, L> {
    if (value === this.value) {
      return this
    }
    return new Vertex({succ: this.succ, pred: this.pred, value})
  }

  removeAllLinksTo(key: K): Vertex<K, V, L> {
    const succ = this.succ.remove(key)
    const pred = this.pred.remove(key)
    if (succ === this.succ && pred === this.pred) {
      return this
    }
    return new Vertex({succ, pred, value: this.value})
  }

  linkTo(key: K, value: L): Vertex<K, V, L> {
    return new Vertex(
      {succ: this.succ.set(key, value), pred: this.pred, value: this.value}
    )
  }

  linkFrom(key: K): Vertex<K, V, L> {
    return new Vertex(
      {succ: this.succ, pred: this.pred.add(key), value: this.value}
    )
  }

  unlinkTo(key: K): Vertex<K, V, L> {
    return new Vertex(
      {succ: this.succ.remove(key), pred: this.pred, value: this.value}
    )
  }

  unlinkFrom(key: K): Vertex<K, V, L> {
    return new Vertex(
      {succ: this.succ, pred: this.pred.remove(key), value: this.value}
    )
  }

}

/**
 * A valued link linking two vertices together.
 */
class ValuedLink<V, L> extends LightweightImmutable {

  origin: V;
  target: V;
  value: L;

  constructor(origin: V, target: V, value: L) {
    super({origin, target, value});
  }

}

let emptyDigraph: any = null;

/**
 * Immutable Directed Graph, with simple arcs and valued vertices.
 */
export default class Digraph<K, V, L> extends LightweightImmutable {

  _vertices: immutable._Map<K, Vertex<K, V, L>>;
  order: number;

  constructor(vertices: immutable._Map<K, Vertex<K, V, L>>) {
    super({_vertices: vertices, order: vertices.size})
  }

  static empty(): Digraph<K, V, L> {
    if (emptyDigraph == null) {
      emptyDigraph = new Digraph(immutable.Map())
    }
    return emptyDigraph
  }

  /**
   * Set a vertex value by its key.
   */
  set(key: K, value: V): Digraph<K, V, L> {
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
   * Get the value of a link between two vertices. Return `undefined` if there
   * is no such link.
   */
  getLink(originKey: K, targetKey: K): ?L {
    const vertex = this._vertices.get(originKey)
    if (vertex == null) {
      return undefined
    }
    return vertex.succ.get(targetKey)
  }

  /**
   * Check if a vertex exists by its key.
   */
  has(key: K): boolean {
    return this._vertices.has(key)
  }

  /**
   * Return `true` if there's a link between these two vertices. This is
   * different from calling `getLink(a, b) !== undefiend ` because a link can
   * have a value of `undefined`.
   */
  hasLink(originKey: K, targetKey: K): boolean {
    const vertex = this._vertices.get(originKey)
    if (vertex == null) {
      return false
    }
    return vertex.succ.has(targetKey)
  }

  /**
   * Remove a vertex and all links from/to it.
   */
  remove(key: K): Digraph<K, V, L> {
    if (!this._vertices.has(key)) {
      return this
    }
    const vertices = this._vertices.map(vertex => vertex.removeAllLinksTo(key))
    return new Digraph(vertices.remove(key))
  }

  _updateLinks(
    originKey: K,
    targetKey: K,
    updater: (origin: Vertex<K, V, L>, target: Vertex<K, V, L>) => {
      origin: Vertex<K, V, L>,
      target: Vertex<K, V, L>,
    }
  ): Digraph<K, V, L> {
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
  link(originKey: K, targetKey: K, value: L): Digraph<K, V, L> {
    return this._updateLinks(originKey, targetKey, (origin, target) => ({
      origin: origin.linkTo(targetKey, value),
      target: target.linkFrom(originKey),
    }))
  }

  /**
   * Remove an arc from one vertex to another.
   */
  unlink(originKey: K, targetKey: K): Digraph<K, V, L> {
    return this._updateLinks(originKey, targetKey, (origin, target) => (
      {origin: origin.unlinkTo(targetKey), target: target.unlinkFrom(originKey)}
    ))
  }

  /**
   * Get all the successors of this element.
   */
  following(key: K): immutable._Iterable_Keyed<K, ValuedLink<V, L>> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Iterable.Keyed()
    }
    return vertexData.succ.toSeq().map(
      (value, targetKey) => {
        const target = nullthrows(this._vertices.get(targetKey))
        return new ValuedLink(
          vertexData.value,
          target.value,
          value
        )
      }
    )
  }

  /**
   * Get all the predecessors of this element.
   */
  preceding(key: K): immutable._Iterable_Keyed<K, ValuedLink<V, L>> {
    const vertexData = this._vertices.get(key)
    if (vertexData == null) {
      return immutable.Iterable.Keyed()
    }
    return immutable.Iterable.Keyed(vertexData.pred.toSeq().map(
      originKey => {
        const origin = nullthrows(this._vertices.get(originKey))
        return [originKey, new ValuedLink(
          origin.value,
          vertexData.value,
          (origin.succ.get(key): any)
        )]
      }
    ))
  }

  some(pred: (value: V, key: K, graph: Digraph<K, V, L>) => boolean): boolean {
    return this._vertices.some((vertex, key) => pred(vertex.value, key, this))
  }

  toSeq(): immutable._Iterable_Keyed<K, V> {
    return this._vertices.toSeq().map(vertex => vertex.value)
  }

  map(
    iter: (value: V, key: K, graph: Digraph<K, V, L>) => V
  ): Digraph<K, V, L> {
    return new Digraph(this._vertices.map((vertex, key) => (
      vertex.setValue(iter(vertex.value, key, this))
    )))
  }

  isEmpty(): boolean {
    return this.order === 0
  }

}
