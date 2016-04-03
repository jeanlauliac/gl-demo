/* @flow */

'use strict';

import type {AgentConfig} from './Agent';

import Agent from './Agent';
import nopt from 'nopt';

export default function cli(configure: () => AgentConfig) {
  if (process.getuid() <= 0) {
    process.stderr.write('Cowardly refusing to execute as root.\n');
    return process.exit(124);
  }
  const opts = nopt({verbose: Boolean, once: Boolean})
  const agent = new Agent(configure(opts));
  agent.update({type: 'start'});
}
