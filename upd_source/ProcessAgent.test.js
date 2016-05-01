/* @flow */

'use strict';

import ProcessAgent from './ProcessAgent';
import immutable from 'immutable';
import * as process from './process';
import sinon from 'sinon';
import tap from 'tap';

tap.test('ProcessAgent', t => {

  const kill = sinon.spy();
  const procs = {};
  const spawn = sinon.spy(command => {
    const proc = {
      on(event, fun) {event === 'exit' && (this.exit = fun)},
      kill() {kill(command)},
    };
    procs[command] = proc;
    return proc;
  });
  const agent = new ProcessAgent(spawn);
  const onExit = sinon.spy();
  agent.on('exit', onExit);

  const proc1 = process.create('ls', immutable.List(['-la']));
  agent.update(immutable.Map({proc1}));
  t.ok(spawn.calledOnce &&
    spawn.calledWith(proc1.command, proc1.args.toArray()));
  t.ok(!kill.called);

  const proc2 = process.create('grep', immutable.List(['beep']));
  agent.update(immutable.Map({proc1, proc2}));
  t.ok(spawn.calledWith(proc2.command, proc2.args.toArray()));
  t.ok(!kill.called);

  agent.update(immutable.Map({proc2}));
  t.ok(kill.calledWith(proc1.command));

  procs[proc2.command].exit();
  t.ok(onExit.calledOnce && onExit.calledWith('proc2'));

  agent.update(immutable.Map({}));
  t.ok(!kill.calledWith(proc2.command));

  t.end();

});
