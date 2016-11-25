/**
 * This is the core entry point to both CLI and server. Having a single point
 * of entry allows us to have the user-defined configuration script to be able
 * to be run to spawn the server.
 *
 * @flow
 */

'use strict';

import type {AgentCLIOptions, AgentConfig} from './agent-state';

import Terminal from './Terminal';
import nullthrows from './nullthrows';
import Agent from './Agent';
import {spawn} from 'child_process';
import dnode from 'dnode';
import leftPad from 'left-pad';
import os from 'os';
import fs from 'fs';
import path from 'path';

const terminal = Terminal.get();

function getServerPIDFilePath(rootDirPath) {
  return path.join(rootDirPath, '.upd', 'server.pid')
}

function getServerPID(pidFilePath) {
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

function stopServer({rootDirPath}, callback) {
  const pidFilePath = getServerPIDFilePath(rootDirPath);
  const pid = getServerPID(pidFilePath);
  if (pid == null) {
    terminal.log('No server is running.');
    return callback();
  }
  terminal.setPrompt('Stopping server (pid: %s)...', pid);
  process.kill(pid);
  const waitForPID = (retries) => setTimeout(() => {
    if (getServerPID(pidFilePath) == null) {
      terminal.setFinalPrompt('Stopping server (pid: %s), done.', pid);
      return callback();
    }
    if (retries > 0) {
      waitForPID(retries - 1);
      return;
    }
    terminal.setFinalPrompt('Stopping server (pid: %s), failed.', pid);
    terminal.log(
      'You need to manually kill process %s and clean up the file: %s',
      pid,
      path.relative('.', pidFilePath),
    );
    callback(new Error('could not stop server'));
  }, 500);
  waitForPID(10);
}

function startServer(rootDirPath, callback) {
  const pidFilePath = getServerPIDFilePath(rootDirPath);
  const pid = getServerPID(pidFilePath);
  if (pid != null) {
    return callback();
  }
  terminal.setPrompt('Starting server...');
  const server = spawn(
    process.execPath,
    [process.mainModule.filename, 'server'],
    {
      detached: true,
      stdio: 'ignore',
    },
  );
  /* $FlowIssue: Flow doesn't know */
  server.unref();
  const waitForPID = (retries) => setTimeout(() => {
    const pid = getServerPID(pidFilePath);
    if (pid != null) {
      terminal.setFinalPrompt('Starting server, done (pid: %s).', pid);
      callback();
      return;
    }
    if (retries > 0) {
      waitForPID(retries - 1);
      return;
    }
    terminal.setFinalPrompt('Starting server, failed.', pid);
    callback(new Error('Failed to start server'));
  }, 500);
  waitForPID(10);
}

function runServer({rootDirPath, configure, opts}) {
  const pidFilePath = getServerPIDFilePath(rootDirPath);
  try {fs.mkdirSync(path.join(rootDirPath, '.upd'))} catch (error) {
    if (error.code !== 'EEXIST') {throw error;}
  }
  process.on('exit', () => fs.unlinkSync(pidFilePath));
  fs.writeFileSync(pidFilePath, process.pid.toString(), {flag: 'wx'});
  const agent = new Agent(configure(opts), spawn);
}

const SECS_IN_A_NANOSEC = 1 / Math.pow(10, 9);

function formatHRTime(hrtime) {
  const decimalPart = (hrtime[1] * SECS_IN_A_NANOSEC).toString();
  return `${hrtime[0]}.${decimalPart.substr(2, 2)}s`;
}

function getProgressBar(ratio: number, length: number) {
  const blockCount = Math.floor(ratio * length);
  return (
    '\u2593'.repeat(blockCount) +
    '\u2591'.repeat(length - blockCount)
  );
}

function updatePrompt(status, totalCount, startHRTime) {
  if (totalCount === 0) {
    terminal.setPrompt('All files are already up-to-date.');
    return true;
  }
  const updatedCount = totalCount - status.staleFileCount;
  const percent = Math.ceil((updatedCount / totalCount) * 100);
  const isDone = totalCount === updatedCount;
  const finish = isDone
    ? `, done in ${formatHRTime(process.hrtime(startHRTime))}.`
    : (
      status.scheduledFileCount === 0
        ? `, failed! (${formatHRTime(process.hrtime(startHRTime))})`
        : '...'
    );
  const progress = isDone ? ''
    : getProgressBar(updatedCount / totalCount, 20) + ' ';
  terminal.setPrompt(
    `Updating ${progress}${percent}% ` +
    `(${updatedCount}/${totalCount})${finish}`
  );
}

const COMMAND_HANDLERS = new Map([
  // Start server if necessary.
  ['start', ({rootDirPath, configure}) => {
    startServer(rootDirPath, error => {
      if (error) {
        throw error;
      }
    });
  }],
  // Kill the server process.
  ['stop', args => stopServer(args, error => {
    if (error) {
      throw error;
    }
  })],
  // Start server if necessary, and ask it to update the files.
  ['update', ({rootDirPath, opts}) => {
    const startHRTime = process.hrtime();
    startServer(rootDirPath, error => {
      if (error) {
        throw error;
      }
      const d = dnode.connect(5004);
      d.on('remote', remote => {
        remote.update((error, status) => {
          if (error) {
            throw error;
          }
          const totalFileCount = status.scheduledFileCount;
          const updateStatus = status => {
            status.processes.forEach(output => {
              if (output.stdout.length === 0 && output.stderr.length === 0) {
                return;
              }
              const header = [
                Array(5).join('\u2500'),
                '\u2524 ',
                output.desc.processDesc.command,
                ` (${path.relative(rootDirPath, output.targetPath)}) `,
                '\u251C',
              ].join('');
              /* $FlowIssue: special field */
              const {columns} = process.stdout;
              terminal.log([
                header,
                Array(columns - header.length).join('\u2500'),
              ].join(''));
              terminal.write(output.stdout);
              terminal.write(output.stderr);
            });
            updatePrompt(status, totalFileCount, startHRTime);
            if (status.scheduledFileCount === 0) {
              terminal.logPrompt();
              d.end();
              if (status.staleFileCount > 0) {
                process.exitCode = 2;
              }
              return;
            }
            remote.waitForStatus((error, status) => {
              if (error) {
                throw error;
              }
              updateStatus(status);
            });
          };
          updateStatus(status);
        });
      });
    });
  }],
  ['restart', args => {
    stopServer(args, error => {
      if (error) {
        throw error;
      }
      startServer(args.rootDirPath, error => {
        if (error) {
          throw error;
        }
      });
    });
  }],
  // Start a server and wait for instructions.
  ['server', runServer],
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
