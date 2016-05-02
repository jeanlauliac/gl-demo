declare module 'glob' {
  declare function sync(glob: string): Array<string>;
}

declare module 'mkdirp' {
  declare function sync(path: string): void;
}

declare module 'nopt' {
  declare function exports(opts: Object): Object;
}

declare module 'chokidar' {
  declare function watch(path: string|Array<string>): any;
}

declare module 'tap' {
  declare function test(name: string, cb: (t: any) => void): void;
}

declare module 'sinon' {
  declare function spy(): any;
}

declare module 'immutable' {

  declare interface __Common {
    hashCode(): number;
    equals(other: any): boolean;
  }

  declare interface __Common_KeyValue<K, V> extends __Common {
    count(): number;
    entries(): Iterator<[K, V]>;
    entrySeq(): ImmIndexedIterable<[K, V]>;
    every(pred: (value: V, key: K) => boolean): boolean;
    forEach(iter: (value: V, key: K) => ?boolean): number;
    get(key: K): V | void;
    has(key: K): boolean;
    keys(): Iterator<K>;
    keySeq(): ImmIndexedIterable<K>;
    isEmpty(): boolean;
    join(glue: string): string;
    reduce<T>(iter: (acc: T, value: V, key: K) => T, init?: T): T;
    some(pred: (value: V, key: K) => boolean): boolean;
    toArray(): Array<V>;
    toList(): ImmList<V>;
    toMap(): ImmMap<K, V>;
    toSet(): ImmSet<K>;
  }

  declare class ImmKeyedIterable<K, V> extends __Common_KeyValue<K, V> {
    filter(pred: (value: V, key?: K) => boolean): ImmKeyedIterable<K, V>;
    flatMap<T, U>(
      mapper: (value: V, key: K) => __Common_KeyValue<T, U>,
    ): ImmKeyedIterable<T, U>;
    map<T>(iter: (value: V, key: K) => T): ImmKeyedIterable<K, T>;
    mapKeys<T>(mapper: (key: K, value: V) => T): ImmKeyedIterable<T, V>;
    size: ?number;
    toSeq(): ImmKeyedIterable<K, V>;
  }

  declare class ImmMap<K, V> extends __Common_KeyValue<K, V> {
    delete(key: K): ImmMap<K, V>;
    filter(pred: (value: V, key?: K) => boolean): ImmMap<K, V>;
    map<T>(iter: (value: V, key: K) => T): ImmMap<K, T>;
    remove(key: K): ImmMap<K, V>;
    set(key: K, value: V): ImmMap<K, V>;
    size: number;
    toSeq(): ImmKeyedIterable<K, V>;
    withMutations(mutator: (mutableMap: Object) => void): ImmMap<K, V>;
  }

  declare class ImmIndexedIterable<V> extends __Common_KeyValue<number, V> {
    map<T>(iter: (value: V, key: number) => T): ImmIndexedIterable<T>;
  }

  declare class ImmSet<K> extends __Common_KeyValue<K, K> {
    add(key: K): ImmSet;
    delete(key: K): ImmSet<K>;
    filter(pred: (key: K) => boolean): ImmSet<K>;
    remove(key: K): ImmSet<K>;
    subtract(from: ImmSet<K>): ImmSet<K>;
    toSeq(): ImmKeyedIterable<K, K>;
  }

  declare class ImmList<V> extends __Common_KeyValue<number, V> {
    concat(values: Array<V> |
      Iterator<V> |
      ImmList<V>
    ): ImmList<V>;
    delete(key: number): ImmList<V>;
    indexOf(value: V): number;
    map<T>(iter: (value: V, key: number) => T): ImmList<T>;
    take(amount: number): ImmList<V>;
    toSeq(): ImmIndexedIterable<V>;
  }

  declare class IterableImpl {
    Indexed<V>(
      values?: Array<V> |
        Iterator<V> |
        ImmIndexedIterable<V>
    ): ImmIndexedIterable<V>;
    Keyed<K, V>(
      values?: Array<[K, V]> |
        Iterator<[K, V]> |
        ImmIndexedIterable<[K, V]>
    ): ImmKeyedIterable<K, V>;
  }

  declare var Iterable: IterableImpl;
  declare function Set<V>(): ImmSet<V>;
  declare function Map<K, V>(): ImmMap<K, V>;
  declare function List<V>(): ImmList<V>;

  declare function is(left: any, right: any): boolean;

}
