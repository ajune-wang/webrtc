#!/usr/bin/env vpython3

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

from contextlib import contextmanager

import os
import tempfile
import unittest

# pylint: disable=invalid-name
gtest_parallel_wrapper = __import__('gtest-parallel-wrapper')


@contextmanager
def TemporaryDirectory():
  tmp_dir = tempfile.mkdtemp()
  yield tmp_dir
  os.rmdir(tmp_dir)


class GtestParallelWrapperTest(unittest.TestCase):
  @classmethod
  def _Expected(cls, gtest_parallel_args):
    return ['--shard_index=0', '--shard_count=1'] + gtest_parallel_args

  def testJsonTestResults(self):
    result = gtest_parallel_wrapper.ParseArgs(
        ['--isolated-script-test-output', '/tmp/foo', 'exec'])
    expected = self._Expected(['--dump_json_test_results=/tmp/foo', 'exec'])
    self.assertEqual(result, expected)

  def testShortArg(self):
    result = gtest_parallel_wrapper.ParseArgs(['-d', '/tmp/foo', 'exec'])
    expected = self._Expected(['-d', '/tmp/foo', 'exec'])
    self.assertEqual(result, expected)

  def testBoolArg(self):
    result = gtest_parallel_wrapper.ParseArgs(
        ['--gtest_also_run_disabled_tests', 'exec'])
    expected = self._Expected(['--gtest_also_run_disabled_tests', 'exec'])
    self.assertEqual(result, expected)

  def testNoArgs(self):
    result = gtest_parallel_wrapper.ParseArgs(['exec'])
    expected = self._Expected(['exec'])
    self.assertEqual(result, expected)

  def testStandardWorkers(self):
    """Check integer value is passed as-is."""
    result = gtest_parallel_wrapper.ParseArgs(['--workers', '17', 'exec'])
    expected = self._Expected(['--workers', '17', 'exec'])
    self.assertEqual(result, expected)


if __name__ == '__main__':
  unittest.main()
