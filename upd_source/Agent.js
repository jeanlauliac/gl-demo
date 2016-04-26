/* @flow */

'use strict';

import type {FileAdjacencyList, FilePath} from './file-adjacency-list';
import type {AgentEvent} from './agent-event';
import type {AgentState} from './agent-state';

import ProcessAgent from './ProcessAgent';
import {update} from './agent-state';
import immutable from 'immutable';
import util from 'util';

type AgentCLIConfig = {
  // Terminate the agent once everything is built.
  once: boolean,
  // Print commands.
  verbose: boolean,
};

export type BuildResult = {
  result: 'built' | 'failed',
  dynamicDependencies: ImmList<FilePath>,
};

export type AgentConfig = AgentCLIConfig & {
  // What file is used to build other files.
  fileAdjacencyList: FileAdjacencyList,
  // What should be used to build a each particular file.
  fileBuilders: ImmMap<FilePath, (
    filePath: FilePath,
    sourceFilePaths: ImmList<FilePath>,
    helpers: Helpers,
  ) => Promise<BuildResult>>,
};

/**
 * Manage the state of the build at any point in time.
 */
export default class Agent {

  config: AgentConfig;
  state: AgentState;
  processAgent: ProcessAgent;

  constructor(config: AgentConfig) {
    Object.defineProperty(this, 'config', {value: config});
    this.state = Object.freeze({
      staleFiles: immutable.Set(),
    });
    this.processAgent = new ProcessAgent();
  }

  verboseLog(...args) {
    if (this.config.verbose) {
      console.log('[verbose] %s', util.format(...args));
    }
  }

  update(event: AgentEvent): void {
    this.verboseLog('Event `%s`', event.type);
    const newState = update(this.config, this.state, event);
    this.processAgent.update(newState.processes);
    this.state = newState;
  }

}
