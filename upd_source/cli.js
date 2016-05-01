/* @flow */

'use strict';

import type {AgentConfig} from './agent-state';

import Agent from './Agent';
import nopt from 'nopt';
import {spawn} from 'child_process';

export default function cli(configure: () => AgentConfig): Agent {
  if (process.getuid() <= 0) {
    process.stderr.write('Cowardly refusing to execute as root.\n');
    return process.exit(124);
  }
  const opts = nopt({verbose: Boolean, once: Boolean})
  const agent = new Agent(configure(opts), spawn);
  return agent;
}
