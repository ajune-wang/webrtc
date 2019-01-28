#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Invoke clang-tidy tool.

Usage: clang_tidy.py file.cc [clang-tidy-args...]

Just a proof of concept!
We use an embedded clang-tidy whose version doesn't match clang++.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
#pylint: disable=relative-import
from presubmit_checks_lib.build_helpers import GetClangTidyPath, \
  GetCompilationCommand, ValidateCC


CHECKER_OPTION = '-checks=*'


def Process(filepath, args):
  # Build directory is needed to gather compilation flags.
  # Create a temporary one (instead of reusing an existing one)
  # to keep the CLI simple and unencumbered.
  out_dir = tempfile.mkdtemp('clang_tidy')

  try:
    gn_args = []  # Use default build.
    command = GetCompilationCommand(filepath, gn_args, out_dir)

    # Remove warning flags. They aren't needed and they cause trouble
    # when clang-tidy doesn't match most recent clang.
    # Same battle for -f (e.g. -fcomplete-member-pointers).
    command = [arg for arg in command if not (arg.startswith('-W') or
                                              arg.startswith('-f'))]

    # Path from build dir.
    rel_path = os.path.relpath(os.path.abspath(filepath), out_dir)

    # Replace clang++ by clang-tidy
    command[0:1] = [GetClangTidyPath(),
                    CHECKER_OPTION,
                    rel_path] + args + ['--']  # Separator for clang flags.
    print "Running: %s" % ' '.join(command)
    # Run from build dir so that relative paths are correct.
    p = subprocess.Popen(command, cwd=out_dir,
                         stdout=sys.stdout, stderr=sys.stderr)
    p.communicate()
    return p.returncode
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)


def Main():
  description = (
    "Run clang-tidy on single cc file.\n"
    "Use flags, defines and include paths as in default debug build.\n"
    "WARNING, this is a POC version with rough edges.")
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('filepath',
                      help='Specifies the path of the .cc file to analyze.',
                      type=ValidateCC)
  parser.add_argument('args',
                      nargs=argparse.REMAINDER,
                      help='Arguments passed to clang-tidy')
  parsed_args = parser.parse_args()
  return Process(parsed_args.filepath, parsed_args.args)


if __name__ == '__main__':
  sys.exit(Main())
