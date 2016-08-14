/* @flow */

'use strict';

import {create, nextOnes, update} from './pending_directories';
import * as immutable from 'immutable';
import tap from 'tap';
import sinon from 'sinon';

tap.test('pending-directories', t => {

  t.test('nextOnes', t => {
    t.similar(nextOnes({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set(),
    }).toArray(), []);
    t.similar(nextOnes({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set([
        'foo.js',
        'bar.js',
      ]),
    }).toArray(), []);
    t.similar(nextOnes({
      existingDirectories: immutable.Set(),
      targetPaths: immutable.Set([
        'some/nested/path/foo.js',
      ]),
    }).toArray(), ['some']);
    t.similar(nextOnes({
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
    setTimeout(() => {
      t.ok(dispatch.calledOnce);
      t.similar(dispatch.args[0][0], {
        directoryPath: 'some/nested',
        type: 'directory-created',
      });
      t.end();
    }, 0);
  });

  t.test('update', t => {
    const dispatch = sinon.spy();
    const createDirectory = sinon.spy(() => Promise.resolve());
    t.similar(update({
      existingDirectories: immutable.Set(['some', 'some/nested']),
      pendingDirectories: immutable.Set(['some/nested']),
      targetPaths: immutable.Set(['some/nested/path/foo.js']),
      dispatch,
      createDirectory,
      event: {directoryPath: 'some/nested', type: 'directory-created'},
    }).toArray(), ['some/nested/path']);
    t.ok(createDirectory.calledOnce);
    t.ok(createDirectory.calledWith('some/nested/path'));
    t.ok(dispatch.notCalled);
    setTimeout(() => {
      t.ok(dispatch.calledOnce);
      t.similar(dispatch.args[0][0], {
        directoryPath: 'some/nested/path',
        type: 'directory-created',
      });
      t.end();
    }, 0);
  });

  t.end();

});
