'use strict';

import glob from 'glob';

glob('./src/*.test.js', (error, files) => {
  if (error) {
    throw error;
  }
  files.forEach(filePath => require(filePath));
});
