/* @flow */

'use strict'

import LightweightImmutable from './LightweightImmutable'

type FileFreshness = 'fresh' | 'updating' | 'stale';
type FileType = 'dep' | 'program' | 'object' | 'source';

/**
 * Describes the status of a file part of the update.
 */
export default class FileStatus extends LightweightImmutable {

  /**
   * Does it need to be refreshed to complete the update? Stale files might not
   * even exist in the filesystem.
   */
  freshness: FileFreshness;
  /**
   * The nature of the file. It'll use different stategies to update the file
   * depending on its type.
   */
  type: FileType;

  static create(freshness: FileFreshness, type: FileType): FileStatus {
    return new FileStatus({freshness, type})
  }

  setFreshness(freshness: FileFreshness): FileStatus {
    return new FileStatus({freshness, type: this.type})
  }

}
