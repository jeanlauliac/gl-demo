/* @flow */

'use strict';

import type {FilePath} from './file_path';
import type {ProcessDesc} from './process_desc';

export type UpdateProcessDesc = {
  // The process to run so as to update the file.
  processDesc: ProcessDesc,
  // A Makefile-looking file generated as output of the process that describe
  // additional files
  dynamicDependenciesFilePath: ?FilePath,
};

export function create(
  processDesc: ProcessDesc,
  dynamicDependenciesFilePath?: FilePath,
): UpdateProcessDesc {
  return {processDesc, dynamicDependenciesFilePath};
}
