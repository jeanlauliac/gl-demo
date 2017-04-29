#!/usr/bin/env node

'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');

function runTestDir(name, dirPath) {
  const result = child_process.spawnSync(
    process.execPath,
    [path.resolve(dirPath, 'test.js')],
    {cwd: dirPath}
  );
  if (result.stdout.length > 0) {
    console.error(`==== [${name}] stdout ====`);
    process.stderr.write(result.stdout);
  }
  if (result.stderr.length > 0) {
    console.error(`==== [${name}] stderr ====`);
    process.stderr.write(result.stderr);
  }
  return result.signal == null && result.status == 0;
}

(function main() {
  console.log('TAP version 13');
  const ents = fs.readdirSync(__dirname);
  let ix = 0;
  for (const ent of ents) {
    const dirPath = path.resolve(__dirname, ent);
    const st = fs.lstatSync(dirPath);
    if (st.isDirectory(dirPath)) {
      ++ix;
      const good = runTestDir(ent, dirPath);
      console.log(`${good ? 'ok' : 'not ok'} ${ix} - e2e test '${ent}'`);
    }
  }
  console.log(`1..${ix}`);
})();
