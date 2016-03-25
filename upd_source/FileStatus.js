/* @flow */

'use strict'

export type Freshness = 'fresh' | 'updating' | 'stale';
export type Type = 'program' | 'object' | 'none';

/**
 * Describes the status of a file part of the update.
 */
export type Status = {
  /**
   * Does it need to be refreshed to complete the update? Stale files might not
   * even exist in the filesystem.
   */
  freshness: Freshness;
  /**
   * The nature of the file. It'll use different stategies to update the file
   * depending on its type.
   */
  type: Type;
};

export function create(freshness: Freshness, type: Type): Status {
  return {freshness, type};
}

export function set(fs: Status, freshness: Freshness): Status {
  return {freshness, type: fs.type};
}
