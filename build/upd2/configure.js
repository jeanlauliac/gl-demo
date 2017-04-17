#!/usr/bin/env node

const compile = [
  'clang++ -c -o $out_file -Wall',
  '-std=c++14 -fcolor-diagnostics -MMD -MF $dep_file',
  '-I /usr/local/include $in_files',
].join(' ');

const link = [
  'clang++ -o $out_files -Wall -std=c++14',
  '-fcolor-diagnostics -L /usr/local/lib $in_files',
].join(' ');

require('../updfile').export(upd => {
  const sourceFiles = upd.glob('src/(**/*).cpp');
  const objFiles = upd.rule(compile, sourceFiles, 'dist/$1.o');
  const binaryFile = upd.rule(link, objFiles, 'upd');
  upd.alias('all', binaryFile);
});
