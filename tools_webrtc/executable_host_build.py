#!/usr/bin/env/python

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


from contextlib import contextmanager
from optparse import OptionParser

import os
import shutil
import subprocess
import sys
import tempfile


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir))
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools


def _ParseOpts():
  parser = OptionParser(
      description=('Builds an executalbe for the host machine '
                   'and it stores the result in the ${root_gen_dir}.\n'
                   'The generated executable will have _host suffix.'))
  parser.add_option('--executable_name', help='Name of the executable to build')
  (options, _) = parser.parse_args()
  return options


@contextmanager
def HostBuildDir():
  temp_dir = tempfile.mkdtemp()
  try:
    yield temp_dir
  finally:
    shutil.rmtree(temp_dir)


def _RunCommand(argv, cwd=SRC_DIR, **kwargs):
  subprocess.check_call(argv, cwd=cwd, **kwargs)


def DepotToolPath(*args):
  return os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, *args)


if __name__ == '__main__':
  OPTS = _ParseOpts()
  EXECUTABLE_TO_BUILD = OPTS.executable_name
  EXECUTABLE_FINAL_NAME = OPTS.executable_name + '_host'
  with HostBuildDir() as build_dir:
    _RunCommand([sys.executable, DepotToolPath('gn.py'), 'gen', build_dir])
    _RunCommand([DepotToolPath('ninja'), '-C', build_dir, EXECUTABLE_TO_BUILD])
    shutil.copy(os.path.join(build_dir, EXECUTABLE_TO_BUILD),
                EXECUTABLE_FINAL_NAME)
