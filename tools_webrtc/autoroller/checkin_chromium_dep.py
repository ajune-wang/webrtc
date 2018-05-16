#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


"""Script to automatically add specified chromium dependency to WebRTC repo."""

import argparse
import errno
import json
import logging
import os.path
import shutil
import subprocess
import sys
import tempfile

REMOTE_URL = 'https://chromium.googlesource.com/chromium/src/third_party'

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_SRC_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, os.pardir,
                                                 os.pardir))
DEPENDENCIES_FILE = os.path.join(CHECKOUT_SRC_DIR,
                                 'THIRD_PARTY_CHROMIUM_DEPS.json')
THIRD_PARTY_PATH = os.path.join(CHECKOUT_SRC_DIR, 'third_party')


class DependencyAlreadyCheckedIn(Exception):
  pass


def VarLookup(local_scope):
  return lambda var_name: local_scope['vars'][var_name]


def ParseDepsDict(deps_content):
  local_scope = {}
  global_scope = {
    'Var': VarLookup(local_scope),
    'deps_os': {},
  }
  exec (deps_content, global_scope, local_scope)
  return local_scope


def ParseLocalDepsFile(filename):
  with open(filename, 'rb') as f:
    deps_content = f.read()
  return ParseDepsDict(deps_content)


def RunCommand(command, working_dir=None, ignore_exit_code=False,
    extra_env=None, input_data=None):
  """Runs a command and returns the output from that command.

  If the command fails (exit code != 0), the function will exit the process.

  Returns:
    A tuple containing the stdout and stderr outputs as strings.
  """
  working_dir = working_dir or CHECKOUT_SRC_DIR
  logging.debug('CMD: %s CWD: %s', ' '.join(command), working_dir)
  env = os.environ.copy()
  if extra_env:
    assert all(type(value) == str for value in extra_env.values())
    logging.debug('extra env: %s', extra_env)
    env.update(extra_env)
  p = subprocess.Popen(command,
                       stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, env=env,
                       cwd=working_dir, universal_newlines=True)
  std_output, err_output = p.communicate(input_data)
  p.stdout.close()
  p.stderr.close()
  if not ignore_exit_code and p.returncode != 0:
    logging.error('Command failed: %s\n'
                  'stdout:\n%s\n'
                  'stderr:\n%s\n', ' '.join(command), std_output, err_output)
    sys.exit(p.returncode)
  return std_output, err_output


def LoadThirdPartyRevision():
  deps_filename = os.path.join(CHECKOUT_SRC_DIR, 'DEPS')
  webrtc_deps = ParseLocalDepsFile(deps_filename)
  return webrtc_deps['vars']['chromium_third_party_revision']


def CheckoutRequiredDependency(dep_name, temp_dir):
  third_party_revision = LoadThirdPartyRevision()

  logging.debug('Initializing git repo in %s...', temp_dir)
  RunCommand(['git', 'init'], working_dir=temp_dir)

  logging.debug('Adding remote to %s. It may take some time...', REMOTE_URL)
  RunCommand(['git', 'remote', 'add', '-f', 'origin', REMOTE_URL],
             working_dir=temp_dir)

  logging.debug('Configuring sparse checkout...')
  RunCommand(['git', 'config', 'core.sparseCheckout', 'true'],
             working_dir=temp_dir)
  sparse_checkout_config_path = os.path.join(temp_dir, '.git', 'info',
                                             'sparse-checkout')
  with open(sparse_checkout_config_path, 'wb') as f:
    f.write(dep_name)

  logging.debug('Pulling changes...')
  RunCommand(['git', 'pull', 'origin', 'master'], working_dir=temp_dir)

  logging.debug('Switching to revision %s...', third_party_revision)
  RunCommand(['git', 'checkout', third_party_revision], working_dir=temp_dir)
  return os.path.join(temp_dir, dep_name)


def CopyDependency(dep_name, source_path):
  dest_path = os.path.join(THIRD_PARTY_PATH, dep_name)
  logging.debug('Copying dependency from %s to %s...', source_path, dest_path)
  shutil.copytree(source_path, dest_path)


def AppendToChromiumOwnedDependenciesList(dep_name):
  with open(DEPENDENCIES_FILE, 'rb') as f:
    data = json.load(f)
    dep_list = data.get('dependencies', [])
    dep_list.append(dep_name)
    data['dependencies'] = dep_list

  with open(DEPENDENCIES_FILE, 'wb') as f:
    json.dump(data, f, indent=2, sort_keys=True, separators=(',', ': '))


def AddToGitIndex(dep_name):
  logging.debug('Adding required changes to git index and commit set...')
  dest_path = os.path.join(THIRD_PARTY_PATH, dep_name)
  RunCommand(['git', 'add', dest_path], working_dir=CHECKOUT_SRC_DIR)
  RunCommand(['git', 'add', DEPENDENCIES_FILE], working_dir=CHECKOUT_SRC_DIR)


def CheckinDependency(dep_name, temp_dir):
  dep_path = CheckoutRequiredDependency(dep_name, temp_dir)
  CopyDependency(dep_name, dep_path)
  AppendToChromiumOwnedDependenciesList(dep_name)
  AddToGitIndex(dep_name)
  logging.info('Dependency checked into current working tree and added into\n'
               'git index. You have to commit generated changes and\n'
               'file the CL to complete dependency check in process')


def CheckinDependencyWithNewTempDir(dep_name):
  temp_dir = tempfile.mkdtemp()
  try:
    logging.info('Using temp directory: %s', temp_dir)
    CheckinDependency(dep_name, temp_dir)
  finally:
    shutil.rmtree(temp_dir)


def CheckDependencyNotCheckedIn(dep_name):
  with open(DEPENDENCIES_FILE, 'rb') as f:
    data = json.load(f)
    dep_list = data.get('dependencies', [])
    if dep_name in dep_list:
      raise DependencyAlreadyCheckedIn("Dependency %s has been already checked "
                                       "into WebRTC repo" % dep_name)
  if dep_name in os.listdir(THIRD_PARTY_PATH):
    raise DependencyAlreadyCheckedIn("Directory for dependency %s already "
                                     "exists in third_party" % dep_name)


def main():
  p = argparse.ArgumentParser()
  p.add_argument('-d', '--dependency', required=True,
                 help='Name of chromium dependency to check in.')
  p.add_argument('--temp-dir',
                 help='Temp working directory to use. By default the one '
                      'provided via tempfile will be used')
  p.add_argument('-v', '--verbose', action='store_true', default=False,
                 help='Be extra verbose in printing of log messages.')
  args = p.parse_args()

  if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.INFO)

  CheckDependencyNotCheckedIn(args.dependency)

  if args.temp_dir:
    if not os.path.exists(args.temp_dir):
      # Raise system error "No such file or directory"
      raise OSError(
          errno.ENOENT, os.strerror(errno.ENOENT), args.temp_dir)
    CheckinDependency(args.dependency, args.temp_dir)
  else:
    CheckinDependencyWithNewTempDir(args.dependency)

  return 0


if __name__ == '__main__':
  sys.exit(main())
