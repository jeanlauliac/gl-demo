/* @flow */

'use strict';

import ProcessAgent from './ProcessAgent';
import immutable from 'immutable';
import * as process from './process';
import sinon from 'sinon';
import tap from 'tap';

tap.test('ProcessAgent', t => {

  const kill = sinon.spy();
  const spawn = sinon.spy(command => ({
    kill() {kill(command)},
  }));
  const agent = new ProcessAgent(spawn);
  const proc1 = process.create('ls', ['-la']);
  agent.update(immutable.List([proc1]));
  t.ok(spawn.calledOnce && spawn.calledWith(proc1.command, proc1.args));
  t.ok(!kill.called);

  const proc2 = process.create('grep', ['beep']);
  agent.update(immutable.List([proc1, proc2]));
  t.ok(spawn.calledWith(proc2.command, proc2.args));
  t.ok(!kill.called);

  agent.update(immutable.List([proc2]));
  t.ok(kill.calledWith(proc1.command));

  agent.update(immutable.List([]));
  t.ok(kill.calledWith(proc2.command));

  t.end();

});
