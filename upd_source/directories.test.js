/* @flow */

'use strict';

import {getPending} from './directories';
import immutable from 'immutable';
import tap from 'tap';

tap.test('directories', t => {
  const existing = [''];
  t.similar(getPending(immutable.Set(), immutable.Set()).toArray(), []);
  t.similar(getPending(immutable.Set(), immutable.Set([
    'foo.js',
    'bar.js',
  ])).toArray(), []);
  t.similar(getPending(immutable.Set([
    'some'
  ]), immutable.Set([
    'some/nested/path/foo.js',
    'some/nested/path/dax.js',
    'some/other/nested/path/bar.js',
  ])).toArray(), [
    'some/nested',
    'some/other',
  ]);
  t.end();
});
