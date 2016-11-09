/**
 * This is the core entry point to both CLI and daemon. Having a single point
 * of entry allows us to have the user-defined configuration script to be able
 * to be run to spawn the daemon.
 *
 * @flow
 */

'use strict';

import type {AgentCLIOptions, AgentConfig} from './src/agent-state';

import nullthrows from './src/nullthrows';
import Agent from './src/Agent';
import {spawn} from 'child_process';
import dnode from 'dnode';
import os from 'os';
import fs from 'fs';
import path from 'path';

function getDaemonPIDFilePath(rootDirPath) {
  return path.join(rootDirPath, '.upd', 'daemon.pid')
}

function getDaemonPID(pidFilePath) {
  let pid;
  try {
    pid = parseInt(fs.readFileSync(pidFilePath));
  } catch (error) {
    if (!(error.code === 'ENOENT')) {
      throw error;
    }
    return null;
  }
  return pid;
}

function stopDaemon({rootDirPath}) {
  const pidFilePath = getDaemonPIDFilePath(rootDirPath);
  const pid = getDaemonPID(pidFilePath);
  if (pid == null) {
    console.error('No daemon is running.');
    return;
  }
  console.error('Trying to stop process %s...', pid);
  process.kill(pid);
  const waitForPID = (retries) => setTimeout(() => {
    if (getDaemonPID(pidFilePath) == null) {
      console.error('Daemon has been stopped.');
      return;
    }
    if (retries === 0) {
      console.error('*** Failed to stop daemon.');
      console.error(
        'You need to manually kill %s and clean up the file: %s',
        pid,
        path.relative('.', pidFilePath),
      );
      return;
    }
    waitForPID(retries - 1);
  }, 500);
  waitForPID(10);
}

function startDaemon(rootDirPath, callback) {
  const pidFilePath = getDaemonPIDFilePath(rootDirPath);
  const pid = getDaemonPID(pidFilePath);
  if (pid != null) {
    console.error('Daemon is running.');
    return callback();
  }
  console.error('Starting process...');
  const daemon = spawn(
    process.execPath,
    [process.mainModule.filename, 'daemon'],
    {
      detached: true,
      stdio: 'ignore',
    },
  );
  /* $FlowIssue: Flow doesn't know */
  daemon.unref();
  const waitForPID = (retries) => setTimeout(() => {
    const pid = getDaemonPID(pidFilePath);
    if (pid != null) {
      console.error(`Daemon ${pid} now running.`);
      callback();
      return;
    }
    if (retries === 0) {
      console.error('*** Failed to start daemon.');
      callback(new Error('Failed to start daemon'));
      return;
    }
    waitForPID(retries - 1);
  }, 500);
  waitForPID(10);
}

function runDaemon({rootDirPath, configure, opts}) {
  const pidFilePath = getDaemonPIDFilePath(rootDirPath);
  try {fs.mkdirSync(path.join(rootDirPath, '.upd'))} catch (error) {
    if (error.code !== 'EEXIST') {throw error;}
  }
  process.on('exit', () => fs.unlinkSync(pidFilePath));
  fs.writeFileSync(pidFilePath, process.pid.toString(), {flag: 'wx'});
  const agent = new Agent(configure(opts), spawn);
}

const COMMAND_HANDLERS = new Map([
  // Start daemon if necessary.
  ['start', ({rootDirPath, configure}) => {
    startDaemon(rootDirPath, error => {
      if (error) {
        throw error;
      }
    });
  }],
  // Kill the daemon process.
  ['stop', stopDaemon],
  // Start daemon if necessary, and ask it to update the files.
  ['update', ({rootDirPath}) => {
    startDaemon(rootDirPath, error => {
      if (error) {
        throw error;
      }
      console.log('Starting...');
      const d = dnode.connect(5004);
      d.on('remote', remote => {
        remote.update((e, v) => {
          console.log('Done!', v);
          d.end();
        });
      });
    });
  }],
  // Start a server and wait for instructions.
  ['daemon', runDaemon],
]);

function parseArgv(argv) {
  const opts = {
    command: 'update',
    verbose: false,
    once: false,
    concurrency: os.cpus().length,
  };
  let commandSet = false;
  for (let i = 0; i < argv.length; ++i) {
    if (argv[i].startsWith('--')) {
      switch (argv[i].substr(2)) {
        case 'verbose':
          opts.verbose = true;
          break;
        case 'once':
          opts.once = true;
          break;
        case 'concurrency':
          opts.concurrency = Number.parseInt(argv[i + 1]);
          if (!(opts.concurrency > 0)) {
            console.error('Invalid concurrency argument:', argv[i + 1]);
            return null;
          }
          i++;
          break;
        default:
          console.error('Unknown option:', argv[i]);
          return null;
      }
      continue;
    }
    if (!commandSet) {
      opts.command = argv[i];
      commandSet = true;
      continue;
    }
    console.error('Invalid argument:', argv[i]);
    return null;
  }
  return opts;
}

export default function upd(
  configure: (cliOpts: AgentCLIOptions) => AgentConfig,
) {
  // $FlowIssue: doesn't know about `getuid`.
  if (process.getuid() <= 0) {
    process.stderr.write('The update cannot run as root for safety reasons.\n');
    process.stderr.write('Retry without using `sudo`, or downgrading to a user.\n');
    process.exitCode = 124;
    return;
  }
  const opts = parseArgv(process.argv.slice(2));
  if (opts == null) {
    process.exitCode = 124;
    return;
  }
  const rootDirPath = path.dirname(process.mainModule.filename);
  const handler = COMMAND_HANDLERS.get(opts.command);
  if (!handler) {
    console.error('Unknown command:', opts.command);
    process.exitCode = 124;
    return;
  }
  handler({rootDirPath, configure, opts});
}
