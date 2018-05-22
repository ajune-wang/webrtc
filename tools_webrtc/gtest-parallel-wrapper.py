#!/usr/bin/env python

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
environment variables to the --shard_index and --shard_count flags, and renames
the --isolated-script-test-output flag to --dump_json_test_results.

All flags before '--' will be passed as arguments to gtest-parallel, and
(almost) all flags after '--' will be passed as arguments to the test
executable.
The exception is that --isolated-script-test-output and
--isolated-script-test-chartson-output are expected to be after '--', so they
are processed and removed from there.

If the --store-test-artifacts flag is set, an --output_dir must be also
specified.
The test artifacts will then be stored in a 'test_artifacts' subdirectory of the
output dir, and will be compressed into a zip file once the test finishes
executing.
This is useful when running the tests in swarming, since the output directory
is not known beforehand.

For example:

  gtest-parallel-wrapper.py some_test \
      --some_flag=some_value \
      --another_flag \
      --output_dir=SOME_OUTPUT_DIR
      -- \
      --store-test-artifacts
      --isolated-script-test-output=SOME_DIR \
      --isolated-script-test-perf-output=SOME_OTHER_DIR \
      --foo=bar \
      --baz

Will be converted into:

  python gtest-parallel some_test \
      --shard_count 1 \
      --shard_index 0 \
      --some_flag=some_value \
      --another_flag \
      --output_dir=SOME_OUTPUT_DIR \
      --dump_json_test_results=SOME_DIR \
      -- \
      --test_artifacts_dir=SOME_OUTPUT_DIR/test_artifacts \
      --foo=bar \
      --baz

"""

import argparse
import collections
import os
import shutil
import subprocess
import sys


Args = collections.namedtuple('Args',
                              ['gtest_parallel_args', 'test_env', 'output_dir',
                               'test_artifacts_dir'])


def _CatFiles(file_list, output_file):
  with open(output_file, 'w') as output_file:
    for filename in file_list:
      with open(filename) as input_file:
        output_file.write(input_file.read())
      os.remove(filename)


def ParseArgs(argv=None):
  parser = argparse.ArgumentParser(argv)

  gtest_grp = parser.add_argument_group('Arguments to gtest_parallel')
  gtest_keys = []

  def add_gtest_argument(*args, **kwargs):
    gtest_keys.append(args[-1].lstrip('-'))
    gtest_grp.add_argument(*args, **kwargs)

  add_gtest_argument('-d', '--output_dir')
  add_gtest_argument('-r', '--repeat')
  add_gtest_argument('--retry_failed')
  add_gtest_argument('-w', '--workers')
  add_gtest_argument('--gtest_color')
  add_gtest_argument('--gtest_filter')
  add_gtest_argument('--gtest_also_run_disabled_tests',
                     action='store_true', default=None)
  add_gtest_argument('--timeout')

  # --isolated-script-test-output is used to upload results to the flakiness
  # dashboard. This translation is made because gtest-parallel expects the flag
  # to be called --dump_json_test_results instead.
  parser.add_argument('--isolated-script-test-output',
                      dest='dump_json_test_results')
  gtest_keys.append('dump_json_test_results')

  # Needed when the test wants to store test artifacts, because it doesn't know
  # what will be the swarming output dir.
  parser.add_argument('--store-test-artifacts', action='store_true')

  # No-sandbox is a Chromium-specific flag, ignore it.
  # TODO(oprypin): Remove (bugs.webrtc.org/8115)
  parser.add_argument('--no-sandbox', action='store_true',
                      help=argparse.SUPPRESS)

  parser.add_argument('executable')
  parser.add_argument('executable_args', nargs='*')

  options, executable_args = parser.parse_known_args(argv)

  executable_args = options.executable_args + executable_args

  test_artifacts_dir = None

  if options.store_test_artifacts:
    assert options.output_dir, (
        '--output_dir must be specified for storing test artifacts.')
    test_artifacts_dir = os.path.join(options.output_dir, 'test_artifacts')

    executable_args.insert(0, '--test_artifacts_dir=' + test_artifacts_dir)

  gtest_args = []
  for key in gtest_keys:
    value = getattr(options, key)
    if value is True:
      gtest_args.append('--%s' % key)
    elif value is not None:
      gtest_args.append('--%s=%s' % (key, value))

  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS must be removed from the
  # environment. Otherwise it will be picked up by the binary, causing a bug
  # where only tests in the first shard are executed.
  test_env = os.environ.copy()

  gtest_shard_index = test_env.pop('GTEST_SHARD_INDEX', '0')
  gtest_total_shards = test_env.pop('GTEST_TOTAL_SHARDS', '1')

  gtest_args += [
      '--shard_count=%s' % gtest_total_shards,
      '--shard_index=%s' % gtest_shard_index,
  ]
  gtest_args += [options.executable, '--'] + executable_args

  return Args(gtest_args, test_env, options.output_dir, test_artifacts_dir)


def main():
  webrtc_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  gtest_parallel_path = os.path.join(
      webrtc_root, 'third_party', 'gtest-parallel', 'gtest-parallel')

  gtest_parallel_args, test_env, output_dir, test_artifacts_dir = ParseArgs()

  command = [
      sys.executable,
      gtest_parallel_path,
  ] + gtest_parallel_args

  if output_dir and not os.path.isdir(output_dir):
    os.makedirs(output_dir)
  if test_artifacts_dir and not os.path.isdir(test_artifacts_dir):
    os.makedirs(test_artifacts_dir)

  print 'gtest-parallel-wrapper: Executing command %s' % ' '.join(command)
  sys.stdout.flush()

  exit_code = subprocess.call(command, env=test_env, cwd=os.getcwd())

  if output_dir:
    for test_status in 'passed', 'failed', 'interrupted':
      logs_dir = os.path.join(output_dir, 'gtest-parallel-logs', test_status)
      if not os.path.isdir(logs_dir):
        continue
      logs = [os.path.join(logs_dir, log) for log in os.listdir(logs_dir)]
      log_file = os.path.join(output_dir, '%s-tests.log' % test_status)
      _CatFiles(logs, log_file)
      os.rmdir(logs_dir)

  if test_artifacts_dir:
    shutil.make_archive(test_artifacts_dir, 'zip', test_artifacts_dir)
    shutil.rmtree(test_artifacts_dir)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
