declare module 'glob' {
  declare function sync(glob: string): Array<string>;
}
declare module 'mkdirp' {
  declare function sync(path: string): void;
}
