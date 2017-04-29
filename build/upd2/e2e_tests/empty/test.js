'use strict';

const child_process = require('child_process');
const path = require('path');
const rimraf = require('rimraf');

(function main() {
  const result = child_process.spawnSync(
    path.resolve(__dirname, '../../dist/upd'),
    ['--all'],
    {cwd: __dirname}
  );
  rimraf.sync(path.resolve(__dirname, '.upd'));
  if (result.signal != null) {
    throw Error(`upd exited with signal ${result.signal}`);
  }
  if (result.status != 0) {
    throw Error(`upd exited with code ${result.status}`);
  }
})();
