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

declare module 'immutable' {

  declare interface __Common_KeyValue<K, V> {
    count(): number;
    get(key: K): V;
    has(key: K): boolean;
    some(pred: (value: V, key: K) => boolean): boolean;
    toSeq(): _Iterable_Keyed<K, V>;
  }

  declare class _Iterable_Keyed<K, V> extends __Common_KeyValue<K, V> {
    filter(pred: (value: V, key?: K) => boolean): _Iterable_Keyed<K, V>;
    map(iter: (value: V, key: K) => V): _Iterable_Keyed<K, V>;
    size: ?number;
  }

  declare class _Map<K, V> extends __Common_KeyValue<K, V> {
    filter(pred: (value: V, key?: K) => boolean): _Map<K, V>;
    map(iter: (value: V, key: K) => V): _Map<K, V>;
    set(key: K, value: V): _Map<K, V>;
    size: number;
  }

  declare class _Iterable_Indexed<V> {
    get(key: number): V;
    forEach(iter: (value: V, key: number) => ?boolean): void;
  }

  declare class _Set<K> {
    add(key: K): _Set;
    every(pred: (key: K) => boolean): boolean;
    forEach(iter: (key: K) => ?boolean): void;
    has(key: K): boolean;
    filter(pred: (key: K) => boolean): _Set<K>;
    toArray(): Array<K>;
  }

  declare class _List<V> {}

  declare class IterableImpl {
    Indexed<V>(values: Array<V>): _Iterable_Indexed<V>;
    Keyed<K, V>(values: Array<[K, V]>): _Iterable_Keyed<K, V>;
  }

  declare var Iterable: IterableImpl;
  declare function Set<V>(): _Set<V>;
  declare function Map<K, V>(): _Map<K, V>;
  declare function List<V>(): _List<V>;

}
