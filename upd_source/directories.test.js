/* @flow */

'use strict';

import {create, nextOnes, update} from './directories';
import * as immutable from 'immutable';
import tap from 'tap';
import sinon from 'sinon';

tap.test('directories', t => {

  t.test('nextOnes', t => {
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set(),
    }).toArray(), []);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set([
        'foo.js',
        'bar.js',
      ]),
    }).toArray(), []);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set([
        'some/nested/path/foo.js',
      ]),
    }).toArray(), ['some']);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(
        ['some', {operation: 'none', error: null}],
      ),
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
      statusesByDirectory: immutable.Map(
        ['some', {operation: 'none', error: null}],
      ),
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
        type: 'create-directory-success',
      });
      t.end();
    }, 0);
  });

  t.test('update', t => {
    const dispatch = sinon.spy();
    const createDirectory = sinon.spy(() => Promise.resolve());
    t.similar(update({
      statusesByDirectory: immutable.Map(
        ['some', {operation: 'none', error: null}],
        ['some/nested', {operation: 'create', error: null}],
      ),
      targetPaths: immutable.Set(['some/nested/path/foo.js']),
      dispatch,
      createDirectory,
      event: {directoryPath: 'some/nested', type: 'create-directory-success'},
    }).toArray(), ['some/nested/path']);
    t.ok(createDirectory.calledOnce);
    t.ok(createDirectory.calledWith('some/nested/path'));
    t.ok(dispatch.notCalled);
    setTimeout(() => {
      t.ok(dispatch.calledOnce);
      t.similar(dispatch.args[0][0], {
        directoryPath: 'some/nested/path',
        type: 'create-directory-success',
      });
      t.end();
    }, 0);
  });

  t.end();

});
