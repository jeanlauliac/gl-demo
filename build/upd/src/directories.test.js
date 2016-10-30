/* @flow */

'use strict';

import {create, nextOnes, update} from './directories';
import * as file_path from './file_path';
import * as immutable from 'immutable';
import tap from 'tap';
import sinon from 'sinon';

const TARGET_PATHS = immutable.Set([
  file_path.create('/some/nested/path/foo.js'),
  file_path.create('/another/one.js'),
]);

tap.test('directories', t => {

  t.test('nextOnes', t => {
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set(),
    }).toArray(), []);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set([
        file_path.create('/foo.js'),
        file_path.create('/bar.js'),
      ]),
    }).toArray(), []);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map(),
      targetPaths: immutable.Set([
        file_path.create('/some/nested/path/foo.js'),
      ]),
    }).toArray(), ['/some']);
    t.similar(nextOnes({
      statusesByDirectory: immutable.Map([
        [file_path.create('/some'), {operation: 'none', error: null}],
      ]),
      targetPaths: immutable.Set([
        file_path.create('/some/nested/path/foo.js'),
        file_path.create('/some/nested/path/dax.js'),
        file_path.create('/some/other/nested/path/bar.js'),
      ]),
    }).toArray(), [
      '/some/nested',
      '/some/other',
    ]);
    t.end();
  });

  t.test('create', t => {
    const dispatch = sinon.spy();
    const createDirectory = sinon.spy(() => Promise.resolve());
    t.similar(create({
      targetPaths: TARGET_PATHS,
      dispatch,
      createDirectory,
    }).entrySeq().toArray(), [
      [file_path.create('/some'), {operation: 'create', error: null}],
      [file_path.create('/another'), {operation: 'create', error: null}],
    ], 'create() should return the first paths being created');
    t.ok(createDirectory.calledTwice);
    t.ok(createDirectory.calledWith('/some'));
    t.ok(createDirectory.calledWith('/another'));
    t.ok(dispatch.notCalled);
    setTimeout(() => {
      t.ok(dispatch.calledTwice);
      t.similar(dispatch.args[0][0], {
        directoryPath: '/some',
        type: 'create-directory-success',
      });
      t.similar(dispatch.args[1][0], {
        directoryPath: '/another',
        type: 'create-directory-success',
      });
      t.end();
    }, 0);
  });

  t.test('update', t => {
    const dispatch = sinon.spy();
    const createDirectory = sinon.spy(dirpath =>
      dirpath === '/another'
        ? Promise.reject(new Error('booooo!'))
        : Promise.resolve()
    );
    t.similar(update({
      statusesByDirectory: immutable.Map([
        [file_path.create('/some'), {operation: 'none', error: null}],
        [file_path.create('/some/nested'), {operation: 'create', error: null}],
      ]),
      targetPaths: TARGET_PATHS,
      dispatch,
      createDirectory,
      event: {
        directoryPath: file_path.create('/some/nested'),
        type: 'create-directory-success',
      },
    }).entrySeq().toArray(), [
      ['/some', {operation: 'none', error: null}],
      ['/some/nested', {operation: 'none', error: null}],
      ['/some/nested/path', {operation: 'create', error: null}],
      ['/another', {operation: 'create', error: null}],
    ]);
    t.ok(createDirectory.calledTwice);
    t.ok(createDirectory.calledWith('/some/nested/path'));
    t.ok(createDirectory.calledWith('/another'));
    t.ok(dispatch.notCalled);
    setTimeout(() => {
      t.ok(dispatch.calledTwice);
      t.similar(dispatch.args[0][0], {
        directoryPath: file_path.create('/some/nested/path'),
        type: 'create-directory-success',
      });
      t.similar(dispatch.args[1][0], {
        directoryPath: file_path.create('/another'),
        error: new Error('booooo!'),
        type: 'create-directory-failure',
      });
      t.end();
    }, 0);
  });

  t.end();

});
