#!/usr/bin/env vpython3

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# pylint: disable=invalid-name
"""
This script acts as an interface between the Chromium infrastructure and
gtest-parallel, renaming options and translating environment variables into
flags. Developers should execute gtest-parallel directly.

In particular, this translates the GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS
environment variables to the --shard_index and --shard_count flags, renames
the --isolated-script-test-output flag to --dump_json_test_results,
and interprets e.g. --workers=2x as 2 workers per core.

Flags before '--' will be attempted to be understood as arguments to
gtest-parallel. If gtest-parallel doesn't recognize the flag or the flag is
after '--', the flag will be passed on to the test executable.

--isolated-script-test-perf-output is renamed to
--isolated_script_test_perf_output. The Android test runner needs the flag to
be in the former form, but our tests require the latter, so this is the only
place we can do it.

For example:

  gtest-parallel-wrapper.py some_test \
      --some_flag=some_value \
      --another_flag \
      --output_dir=SOME_OUTPUT_DIR \
      --isolated-script-test-output=SOME_DIR \
      --isolated-script-test-perf-output=SOME_OTHER_DIR \
      -- \
      --foo=bar \
      --baz

Will be converted into:

  vpython3 gtest-parallel \
      --shard_index 0 \
      --shard_count 1 \
      --output_dir=SOME_OUTPUT_DIR \
      --dump_json_test_results=SOME_DIR \
      some_test \
      -- \
      --some_flag=some_value \
      --another_flag \
      --isolated_script_test_perf_output=SOME_OTHER_DIR \
      --foo=bar \
      --baz

"""

import argparse
import os
import subprocess
import sys


def ParseArgs(argv=None):
  parser = argparse.ArgumentParser(argv)

  # --isolated-script-test-output is used to upload results to the flakiness
  # dashboard. This translation is made because gtest-parallel expects the flag
  # to be called --dump_json_test_results instead.
  parser.add_argument('--isolated-script-test-output')
  # --isolated-script-test-perf-output is ignored because performance tests
  # cannot run with gtest-parallel because the perf tests output is shared
  # between all the tests.
  parser.add_argument('--isolated-script-test-perf-output',
                      help=argparse.SUPPRESS)
  # No-sandbox is a Chromium-specific flag, ignore it.
  # TODO(bugs.webrtc.org/8115): Remove workaround when fixed.
  parser.add_argument('--no-sandbox',
                      action='store_true',
                      help=argparse.SUPPRESS)

  args, unrecognized_args = parser.parse_known_args(argv)

  gtest_parallel_args = unrecognized_args
  if args.isolated_script_test_output:
    gtest_parallel_args.insert(
        0, '--dump_json_test_results=%s' % args.isolated_script_test_output)

  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the
  # environment. Otherwise it will be picked up by the binary, causing a bug
  # where only tests in the first shard are executed.
  test_env = os.environ.copy()
  gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
  gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

  gtest_parallel_args.insert(0, '--shard_index=%s' % gtest_shard_index)
  gtest_parallel_args.insert(1, '--shard_count=%s' % gtest_total_shards)

  return gtest_parallel_args


def main():
  webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  gtest_parallel_path = os.path.join(webrtc_root, 'third_party',
                                     'gtest-parallel', 'gtest-parallel')

  gtest_parallel_args = ParseArgs()

  command = [
      sys.executable,
      gtest_parallel_path,
  ] + gtest_parallel_args

  print('gtest-parallel-wrapper: Executing command %s' % ' '.join(command))
  sys.stdout.flush()

  return subprocess.call(command, cwd=os.getcwd())


if __name__ == '__main__':
  sys.exit(main())
