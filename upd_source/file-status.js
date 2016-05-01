/* @noflow */

'use strict';

export type FileFreshness = 'fresh' | 'updating' | 'stale';
export type FileBuilder = 'linker' | 'compiler' | 'none';

/**
 * Describes the status of a file part of the update.
 */
export type FileStatus = {
  /**
   * Does it need to be refreshed to complete the update? Stale files might not
   * even exist in the filesystem.
   */
  freshness: FileFreshness,
  /**
   * The stategy to use to update the file.
   */
  builder: FileBuilder,
};

export function set(fs: FileStatus, freshness: FileFreshness): FileStatus {
  return {freshness, type: fs.type};
}
