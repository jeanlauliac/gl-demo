/* @flow */

'use strict';

import type {AgentCLIOptions, AgentConfig} from './agent-state';

import Agent from './Agent';
import {spawn} from 'child_process';
import nopt from 'nopt';
import os from 'os';

export default function cli(
  configure: (cliOpts: AgentCLIOptions) => AgentConfig,
): Agent {
  if (process.getuid() <= 0) {
    process.stderr.write('Cowardly refusing to execute as root.\n');
    return process.exit(124);
  }
  const opts = nopt({verbose: Boolean, once: Boolean, concurrency: Number});
  if (opts.concurrency == null) {
    opts.concurrency = os.cpus().length;
  }
  const agent = new Agent(configure(opts), spawn);
  return agent;
}
