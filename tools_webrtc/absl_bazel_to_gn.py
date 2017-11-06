#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import optparse
import os
import sys

TARGET_FILE = 'BUILD.gn'

def _ParseArgs():
  """Registers the command-line options."""
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--library_root', type='string',
                    help='Library directory.')
  parser.add_option('--include_dirs', type='string',
                    help='Extra directory to include.')

  options, _ = parser.parse_args()

  if not options.library_root:
    parser.error('You must provide a root!')

  if not os.path.isfile(os.path.join(options.library_root, 'BUILD.bazel')):
    parser.error('Cannot find the library!')

  return options

ROOT_FILE = """config("absl_config") {{
  include_dirs = [ "{root}" ]
  defines = [ "ABSL_ALLOCATOR_NOTHROW=1" ]
}}
"""

TYPES_FILE = """source_set("optional") {
  public = [
    "optional.h",
  ]
  sources = [
    "optional.cc",
  ]
  all_dependent_configs = [ "..:absl_config" ]
}

source_set("span") {
  public = [
    "span.h",
  ]
  all_dependent_configs = [ "..:absl_config" ]
}
"""

MEMORY_FILE = """source_set("memory") {
  public = [ "memory.h" ]
  all_dependent_configs = [ "..:absl_config" ]
}
"""

def main():
  options = _ParseArgs()

  root_file = os.path.join(options.library_root, TARGET_FILE)
  with open(root_file, 'w') as f:
    f.write(ROOT_FILE.format(root=options.include_dirs))

  types_file = os.path.join(options.library_root, 'types', TARGET_FILE)
  with open(types_file, 'w') as f:
    f.write(TYPES_FILE)

  memory_file = os.path.join(options.library_root, 'memory', TARGET_FILE)
  with open(memory_file, 'w') as f:
    f.write(MEMORY_FILE)

if __name__ == '__main__':
  sys.exit(main())
