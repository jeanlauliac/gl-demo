/* @flow */

'use strict';

import type {AgentCLIOptions, AgentConfig} from './agent-state';

import nullthrows from './nullthrows';
import Agent from './Agent';
import {spawn} from 'child_process';
import nopt from 'nopt';
import os from 'os';

export default function cli(
  configure: (cliOpts: AgentCLIOptions) => AgentConfig,
): Agent {
  // $FlowIssue: doesn't know about `getuid`.
  if (process.getuid() <= 0) {
    process.stderr.write('The update cannot run as root for safety reasons.\n');
    process.stderr.write('Retry without using `sudo`, or downgrading to a user.\n');
    // $FlowIssue: doesn't know about `exit`.
    return process.exit(124);
  }
  const opts = nopt({verbose: Boolean, once: Boolean, concurrency: Number});
  if (opts.concurrency == null) {
    opts.concurrency = os.cpus().length;
  }
  const agent = new Agent(configure(opts), spawn);
  return agent;
}
