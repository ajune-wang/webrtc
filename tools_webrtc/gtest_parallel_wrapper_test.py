#!/usr/bin/env python

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest

script = __import__('gtest-parallel-wrapper')  # pylint: disable=invalid-name


class GtestParallelWrapperTest(unittest.TestCase):
  def _CheckArgs(self, argv, gtest_parallel_args, output_dir=None,
                 test_artifacts_dir=None):
    result = script.ParseArgs(argv)
    self.assertListEqual(
        result.gtest_parallel_args,
        ['--shard_index=0', '--shard_count=1'] + gtest_parallel_args)
    self.assertEqual(result.output_dir, output_dir)
    self.assertEqual(result.test_artifacts_dir, test_artifacts_dir)

  def testOverwrite(self):
    self._CheckArgs(
        ['--timeout=123', 'exec', '--timeout', '124'],
        ['--timeout=124', 'exec'])

  def testMixing(self):
    self._CheckArgs(
        ['--timeout=123', '--param1', 'exec', '--param2', '--timeout', '124'],
        ['--timeout=124', 'exec', '--', '--param1', '--param2'])

  def testMixingPositional(self):
    self._CheckArgs(
        ['--timeout=123', 'exec', '--foo1', 'bar1', '--timeout', '124',
         '--foo2', 'bar2'],
        ['--timeout=124', 'exec', '--', '--foo1', 'bar1', '--foo2', 'bar2'])

  def testDoubleDash(self):
    self._CheckArgs(
        ['--timeout', '123', 'exec', '--', '--timeout', '124'],
        ['--timeout=123', 'exec', '--', '--timeout', '124'])
    self._CheckArgs(
        ['--timeout=123', '--', 'exec', '--timeout=124'],
        ['--timeout=123', 'exec', '--', '--timeout=124'])

  def testArtifacts(self):
    self._CheckArgs(
        ['exec', '--store-test-artifacts', '--output_dir', '/tmp/foo'],
        ['--output_dir=/tmp/foo', 'exec', '--',
         '--test_artifacts_dir=/tmp/foo/test_artifacts'],
        output_dir='/tmp/foo', test_artifacts_dir='/tmp/foo/test_artifacts')

  def testJsonTestResults(self):
    self._CheckArgs(
        ['--isolated-script-test-output', '/tmp/foo', 'exec'],
        ['--dump_json_test_results=/tmp/foo', 'exec'])

  def testShortArg(self):
    self._CheckArgs(
        ['-d', '/tmp/foo', 'exec'],
        ['--output_dir=/tmp/foo', 'exec'],
        output_dir='/tmp/foo')

  def testBoolArg(self):
    self._CheckArgs(
        ['--gtest_also_run_disabled_tests', 'exec'],
        ['--gtest_also_run_disabled_tests', 'exec'])

  def testNoArgs(self):
    self._CheckArgs(
        ['exec'],
        ['exec'])

  def testDocExample(self):
    self._CheckArgs(
        ['some_test', '--some_flag=some_value', '--another_flag',
         '--output_dir=SOME_OUTPUT_DIR', '--store-test-artifacts',
         '--isolated-script-test-output=SOME_DIR',
         '--isolated-script-test-perf-output=SOME_OTHER_DIR',
         '--foo=bar', '--baz'],
        ['--output_dir=SOME_OUTPUT_DIR', '--dump_json_test_results=SOME_DIR',
         'some_test', '--',
         '--test_artifacts_dir=SOME_OUTPUT_DIR/test_artifacts',
         '--some_flag=some_value', '--another_flag',
         '--isolated-script-test-perf-output=SOME_OTHER_DIR',
         '--foo=bar', '--baz'],
        output_dir='SOME_OUTPUT_DIR',
        test_artifacts_dir='SOME_OUTPUT_DIR/test_artifacts')


if __name__ == '__main__':
  unittest.main()
