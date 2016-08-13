/* @flow */

'use strict';

import {create, getNextToCreate, update} from './pending-directories';
import immutable from 'immutable';
import tap from 'tap';
import sinon from 'sinon';

tap.test('pending-directories', t => {

  t.test('getNextToCreate', t => {
    t.similar(getNextToCreate({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set(),
    }).toArray(), []);
    t.similar(getNextToCreate({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set([
        'foo.js',
        'bar.js',
      ]),
    }).toArray(), []);
    t.similar(getNextToCreate({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set([
        'some/nested/path/foo.js',
      ]),
    }).toArray(), ['some']);
    t.similar(getNextToCreate({
      existingDirectories: immutable.Set(['some']),
      targetPaths: immutable.Set([
        'some/nested/path/foo.js',
        'some/nested/path/dax.js',
        'some/other/nested/path/bar.js',
      ]),
    }).toArray(), [
      'some/nested',
      'some/other',
    ]);
    t.end();
  });

  t.test('create', t => {
    const dispatch = sinon.spy();
    const createDirectory = sinon.spy(() => Promise.resolve());
    t.similar(create({
      existingDirectories: immutable.Set(['some']),
      targetPaths: immutable.Set(['some/nested/path/foo.js']),
      dispatch,
      createDirectory,
    }).toArray(), ['some/nested']);
    t.ok(createDirectory.calledOnce);
    t.ok(createDirectory.calledWith('some/nested'));
    t.ok(dispatch.notCalled);
    t.end();
  });

  t.end();

});
