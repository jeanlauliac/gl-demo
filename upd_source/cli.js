/* @flow */

'use strict';

import type {AgentCLIOptions, AgentConfig} from './agent-state';

import nullthrows from './nullthrows';
import Agent from './Agent';
import {spawn} from 'child_process';
import os from 'os';

function parseArgv(argv) {
  const opts = {
    verbose: false,
    once: false,
    concurrency: os.cpus().length,
  };
  for (let i = 0; i < argv.length; ++i) {
    switch (argv[i]) {
      case '--verbose':
        opts.verbose = true;
        break;
      case '--once':
        opts.once = true;
        break;
      case '--concurrency':
        opts.concurrency = Number.parseInt(argv[i + 1]);
        if (!(opts.concurrency > 0)) {
          console.error('Invalid concurrency argument:', argv[i + 1]);
          return null;
        }
        i++;
        break;
      default:
        console.error('Unknow argument:', argv[i]);
        return null;
    }
  }
  return opts;
}

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
  const opts = parseArgv(process.argv.slice(2));
  if (opts == null) {
    // $FlowIssue: doesn't know about `exit`.
    return process.exit(124);
  }
  const agent = new Agent(configure(opts), spawn);
  return agent;
}
