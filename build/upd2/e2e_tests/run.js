#!/usr/bin/env node

'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const rimraf = require('rimraf');

const TEST_DIR_PATH = path.resolve(__dirname, '.test_files');

const TESTS = [
  {
    name: "Simple",
    files: {
      "updfile.js": ""
    },
  }
];

(function main() {
  rimraf.sync(TEST_DIR_PATH);
  fs.mkdir(TEST_DIR_PATH);
  fs.mkdir(path.join(TEST_DIR_PATH, '');
  fs.writeFileSync(path.resolve(TEST_DIR_PATH, 'updfile.json'), JSON.stringify({}));
  child_process.execSync(path.join(__dirname, '../dist/upd --all'));
})();
