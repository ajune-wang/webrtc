# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""This script helps to invoke gn and ninja
which lie in depot_tools repository."""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile


def FindSrcDirPath():
  """Returns the abs path to the src/ dir of the project."""
  src_dir = os.path.dirname(os.path.abspath(__file__))
  while os.path.basename(src_dir) != 'src':
    src_dir = os.path.normpath(os.path.join(src_dir, os.pardir))
  return src_dir


SRC_DIR = FindSrcDirPath()
sys.path.append(os.path.join(SRC_DIR, 'build'))
import find_depot_tools


def RunGnCommand(args, root_dir=None):
  """Runs `gn` with provided args and return error if any."""
  try:
    command = [
      sys.executable,
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py')
    ] + args
    subprocess.check_output(command, cwd=root_dir)
  except subprocess.CalledProcessError as err:
    return err.output
  return None


# GN_ERROR_RE matches the summary of an error output by `gn check`.
# Matches "ERROR" and following lines until it sees an empty line or a line
# containing just underscores.
GN_ERROR_RE = re.compile(r'^ERROR .+(?:\n.*[^_\n].*$)+', re.MULTILINE)


def RunGnCheck(root_dir=None):
  """Runs `gn gen --check` with default args to detect mismatches between
  #includes and dependencies in the BUILD.gn files, as well as general build
  errors.

  Returns a list of error summary strings.
  """
  out_dir = tempfile.mkdtemp('gn')
  try:
    error = RunGnCommand(['gen', '--check'] + [out_dir], root_dir)
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)
  return GN_ERROR_RE.findall(error) if error else []


def RunNinjaCommand(args, root_dir=None):
  """Runs ninja quietly. Any failure (e.g. clang not found) is
     silently discarded, since this is unlikely an error in submitted CL."""
  command = [
              os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'ninja')
            ] + args
  p = subprocess.Popen(command, cwd=root_dir,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  out, _ = p.communicate()
  return out


def RunClangStaticAnalyzer(args, root_dir=None):
  """Runs clang_static_analyzer_wrapper.py with passed args."""
  command = [
              sys.executable,
              os.path.join(FindSrcDirPath(),
                           'tools_webrtc',
                           'presubmit_checks_lib',
                           'clang_static_analyzer_wrapper.py')
            ] + args
  p = subprocess.Popen(command, cwd=root_dir,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  # Analyzer diagnostic is in stderr.
  _, err = p.communicate()
  return err


def GetCompilationCommands(root_dir=None):
  """Run ninja compdb tool to get proper flags, defines and include paths."""
  # The compdb tool expect a rule.
  commands = json.loads(RunNinjaCommand(['-t', 'compdb', 'cxx'], root_dir))
  # Turns 'file' field into a key.
  return {v['file']: v for v in commands}
