#!/usr/bin/env vpython3

# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import sys
import subprocess


def main():
  return subprocess.call(['vpython3', 'process_perf_results.py'] + sys.argv[1:])


if __name__ == '__main__':
  sys.exit(main())
