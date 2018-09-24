#!/usr/bin/env python

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

from contextlib import contextmanager

import multiprocessing
import os
import tempfile
import unittest

script = __import__('gtest-parallel-wrapper')  # pylint: disable=invalid-name
GET_WORKERS = script._GetNumberOfWorkers  # pylint: disable=protected-access

@contextmanager
def TemporaryDirectory():
  tmp_dir = tempfile.mkdtemp()
  yield tmp_dir
  os.rmdir(tmp_dir)



class GtestParallelWrapperHelpersTest(unittest.TestCase):


  def testGetWorkersAsIs(self):
    self.assertEqual(GET_WORKERS('12'), 12)

  def testGetTwiceWorkers(self):
    expected = 2 * multiprocessing.cpu_count()
    self.assertEqual(GET_WORKERS('2x'), expected)

  def testGetHalfWorkers(self):
    expected = max(multiprocessing.cpu_count() // 2, 1)
    self.assertEqual(GET_WORKERS('0.5x'), expected)


class GtestParallelWrapperTest(unittest.TestCase):
  @classmethod
  def _Expected(cls, gtest_parallel_args):
    return ['--shard_index=0', '--shard_count=1'] + gtest_parallel_args

  def testOverwrite(self):
    result = script.ParseArgs(['--timeout=123', 'exec', '--timeout', '124'])
    expected = self._Expected(['--timeout=124', 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testMixing(self):
    result = script.ParseArgs(
        ['--timeout=123', '--param1', 'exec', '--param2', '--timeout', '124'])
    expected = self._Expected(
        ['--timeout=124', 'exec', '--', '--param1', '--param2'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testMixingPositional(self):
    result = script.ParseArgs(['--timeout=123', 'exec', '--foo1', 'bar1',
                               '--timeout', '124', '--foo2', 'bar2'])
    expected = self._Expected(['--timeout=124', 'exec', '--', '--foo1', 'bar1',
                               '--foo2', 'bar2'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testDoubleDash1(self):
    result = script.ParseArgs(
        ['--timeout', '123', 'exec', '--', '--timeout', '124'])
    expected = self._Expected(
        ['--timeout=123', 'exec', '--', '--timeout', '124'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testDoubleDash2(self):
    result = script.ParseArgs(['--timeout=123', '--', 'exec', '--timeout=124'])
    expected = self._Expected(['--timeout=123', 'exec', '--', '--timeout=124'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testArtifacts(self):
    with TemporaryDirectory() as tmp_dir:
      output_dir = os.path.join(tmp_dir, 'foo')
      result = script.ParseArgs(['exec', '--store-test-artifacts',
                                 '--output_dir', output_dir])
      exp_artifacts_dir = os.path.join(output_dir, 'test_artifacts')
      exp = self._Expected(['--output_dir=' + output_dir, 'exec', '--',
                            '--test_artifacts_dir=' + exp_artifacts_dir])
      self.assertEqual(result.gtest_parallel_args, exp)
      self.assertEqual(result.output_dir, output_dir)
      self.assertEqual(result.test_artifacts_dir, exp_artifacts_dir)

  def testNoDirsSpecified(self):
    result = script.ParseArgs(['exec'])
    self.assertEqual(result.output_dir, None)
    self.assertEqual(result.test_artifacts_dir, None)

  def testOutputDirSpecified(self):
    result = script.ParseArgs(['exec', '--output_dir', '/tmp/foo'])
    self.assertEqual(result.output_dir, '/tmp/foo')
    self.assertEqual(result.test_artifacts_dir, None)

  def testJsonTestResults(self):
    result = script.ParseArgs(['--isolated-script-test-output', '/tmp/foo',
                               'exec'])
    expected = self._Expected(['--dump_json_test_results=/tmp/foo', 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testShortArg(self):
    result = script.ParseArgs(['-d', '/tmp/foo', 'exec'])
    expected = self._Expected(['--output_dir=/tmp/foo', 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)
    self.assertEqual(result.output_dir, '/tmp/foo')

  def testBoolArg(self):
    result = script.ParseArgs(['--gtest_also_run_disabled_tests', 'exec'])
    expected = self._Expected(['--gtest_also_run_disabled_tests', 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testNoArgs(self):
    result = script.ParseArgs(['exec'])
    expected = self._Expected(['exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testDocExample(self):
    with TemporaryDirectory() as tmp_dir:
      output_dir = os.path.join(tmp_dir, 'foo')
      result = script.ParseArgs([
          'some_test', '--some_flag=some_value', '--another_flag',
          '--output_dir=' + output_dir, '--store-test-artifacts',
          '--isolated-script-test-output=SOME_DIR',
          '--isolated-script-test-perf-output=SOME_OTHER_DIR',
          '--foo=bar', '--baz'])
      expected_artifacts_dir = os.path.join(output_dir, 'test_artifacts')
      expected = self._Expected([
          '--output_dir=' + output_dir, '--dump_json_test_results=SOME_DIR',
          'some_test', '--',
          '--test_artifacts_dir=' + expected_artifacts_dir,
          '--some_flag=some_value', '--another_flag',
          '--isolated-script-test-perf-output=SOME_OTHER_DIR',
          '--foo=bar', '--baz'])
      self.assertEqual(result.gtest_parallel_args, expected)

  def testStandardWorkers(self):
    """ Check integer value is passed as-is."""
    result = script.ParseArgs(['--workers', '17', 'exec'])
    expected = self._Expected(['--workers=17', 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testTwoWorkersPerCpuCore(self):
    result = script.ParseArgs(['--workers', '2x', 'exec'])
    workers = 2 * multiprocessing.cpu_count()
    expected = self._Expected(['--workers=%s' % workers, 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)

  def testUseHalfTheCpuCores(self):
    result = script.ParseArgs(['--workers', '0.5x', 'exec'])
    workers = multiprocessing.cpu_count() // 2
    expected = self._Expected(['--workers=%s' % workers, 'exec'])
    self.assertEqual(result.gtest_parallel_args, expected)


if __name__ == '__main__':
  unittest.main()
