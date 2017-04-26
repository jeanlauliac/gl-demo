#!/usr/bin/env node

'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');

(function main() {
  child_process.execSync(path.join(__dirname, '../dist/upd --all'));
})();
