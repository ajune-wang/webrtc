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
    self.assertListEqual(result.gtest_parallel_args, gtest_parallel_args)
    self.assertEqual(result.output_dir, output_dir)
    self.assertEqual(result.test_artifacts_dir, test_artifacts_dir)

  def testArgs(self):
    self._CheckArgs(
        ['--gtest_color', 'color', 'a', 'b', '--gtest_color', 'color2',
         '--isolated-script-test-output=a'],
        ['--gtest_color=color2', '--dump_json_test_results=a',
         '--shard_count=1', '--shard_index=0', 'a', '--', 'b'])


if __name__ == '__main__':
  unittest.main()
