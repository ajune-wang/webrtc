#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Invoke clang static analyzer.

Usage: clang_static_analyzer_wrapper.py <compiler> [args...]

NB: This is adapted from //build/toolchain/clang_static_analyzer_wrapper.py,
    without compilation step. Also, the set of checkers and options may differ.
"""

import argparse
import subprocess
import sys

# Flags used to enable analysis for Clang invocations.
ANALYZER_ENABLE_FLAGS = [
    '--analyze',
]

# Flags used to configure the analyzer's behavior.
ANALYZER_OPTION_FLAGS = [
    '-fdiagnostics-show-option',
    '-analyzer-opt-analyze-nested-blocks',
    '-analyzer-output=text',
    '-analyzer-config',
    'suppress-c++-stdlib=true',

# List of checkers to execute.
# The full list of checkers can be found at
# https://clang-analyzer.llvm.org/available_checks.html.
    '-analyzer-checker=cplusplus',
    '-analyzer-checker=core',
    '-analyzer-checker=unix',
    '-analyzer-checker=deadcode',
]


# Prepends every element of a list |args| with |token|.
# e.g. ['-analyzer-foo', '-analyzer-bar'] => ['-Xanalyzer', '-analyzer-foo',
#                                             '-Xanalyzer', '-analyzer-bar']
def InterleaveArgs(args, token):
  return list(sum(zip([token] * len(args), args), ()))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--mode',
                      choices=['clang', 'cl'],
                      required=True,
                      help='Specifies the compiler argument convention to use.')
  parser.add_argument('args', nargs=argparse.REMAINDER)
  parsed_args = parser.parse_args()

  prefix = '-Xclang' if parsed_args.mode == 'cl' else '-Xanalyzer'
  cmd = parsed_args.args + ANALYZER_ENABLE_FLAGS + \
        InterleaveArgs(ANALYZER_OPTION_FLAGS, prefix)
  child = subprocess.Popen(cmd, stderr=subprocess.PIPE)
  _, stderr = child.communicate()
  sys.stderr.write(stderr)

  return child.returncode


if __name__ == '__main__':
  sys.exit(main())
