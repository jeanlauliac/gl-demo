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
