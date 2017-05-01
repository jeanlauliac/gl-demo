#!/usr/bin/env node

'use strict';

const fs = require('fs');

function writeDepFile(depFilePath, destFilePath, sourceFilePaths) {
  const sourceFileDepList = sourceFilePaths
    .map(filePath => filePath.replace(/ /g, '\\\\ '))
    .join('\n  ');
  fs.writeFileSync(depFilePath, `${destFilePath}: ${sourceFileDepList}\n`);
}

(function main() {
  const sourceFilePaths = process.argv.slice(4);
  const sources = sourceFilePaths.map(filePath => {
    return '# ' + filePath + '\n' + fs.readFileSync(filePath, 'utf8');
  });
  const result = [
    '# GENERATED FILE\n',
  ].concat(sources).join('');
  const destFilePath = process.argv[2];
  const os = fs.createWriteStream(destFilePath);
  os.write(result);
  os.end();
  writeDepFile(process.argv[3], destFilePath, sourceFilePaths);
})();
