/* @flow */

'use strict';

import * as readline from 'readline';
import util from 'util';

export default class Terminal {

  _currentPrompt: ?string = null;

  _setPrompt(prompt: ?string): ?string {
    if (prompt === this._currentPrompt) {
      return prompt;
    }
    if (this._currentPrompt != null && process.stdout.isTTY) {
      readline.moveCursor(process.stdout, 0, -1);
      readline.clearLine(process.stdout, 0);
    }
    if (prompt != null) {
      console.log(prompt);
    }
    const oldPrompt = this._currentPrompt;
    this._currentPrompt = prompt;
    return oldPrompt;
  }

  /**
   * Change what's currently displayed on the prompt line.
   */
  setPrompt(...args: Array<any>): ?string {
    this._setPrompt(util.format(...args));
  }

  /**
   * Remove any prompt that may be shown.
   */
  clearPrompt(): ?string {
    return this._setPrompt(null);
  }

  /**
   * Transform the prompt currently shown as a log line.
   */
  logPrompt(): ?string {
    const oldPrompt = this._currentPrompt;
    this._currentPrompt = null;
    return oldPrompt;
  }

  setFinalPrompt(...args: Array<any>): ?string {
    const oldPrompt = this.setPrompt(...args);
    this.logPrompt();
    return oldPrompt;
  }

  /**
   * Log information to stdout, pushing down the prompt.
   */
  log(...args: Array<any>): void {
    const prompt = this.clearPrompt();
    console.log(util.format(...args));
    this._setPrompt(prompt);
  }

  write(data: string | Buffer): void {
    const prompt = this.clearPrompt();
    process.stdout.write(data);
    this._setPrompt(prompt);
  }

  static _global: ?Terminal = null;
  static get() {
    if (this._global == null) {
      return new Terminal();
    }
    return this._global;
  }

}
