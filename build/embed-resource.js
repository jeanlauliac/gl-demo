#!/usr/bin/env node

'use strict';

const fs = require('fs');
const path = require('path');

const resourceFilePath = process.argv[3];
const content = fs.readFileSync(resourceFilePath);
const name = path.basename(resourceFilePath).replace('.', '_');
const namespaces = path.dirname(path.relative(
  path.join(__dirname, '..'),
  resourceFilePath
)).split(path.sep);
const fd = fs.openSync(process.argv[2], 'w');
try {
  for (let i = 0; i < namespaces.length; ++i) {
    fs.writeSync(fd, `namespace ${namespaces[i]} {\n`);
  }
  fs.writeSync(fd, `\nunsigned char ${name}[] = {\n  `);
  let first = true;
  for (let i = 0; i < content.length; ++i) {
    if (!first) {
      fs.writeSync(fd, i % 16 === 0 ? ',\n  ' : ', ');
    }
    fs.writeSync(fd, '0x' + content[i].toString(16));
    first = false;
  }
  fs.writeSync(fd, `\n};\n\n${namespaces.map(() => '}\n').join('')}`);
} finally {
  fs.closeSync(fd);
}
