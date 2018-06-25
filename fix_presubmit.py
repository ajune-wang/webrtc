#!/usr/bin/env python

import subprocess
import os
import logging
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_SRC_DIR = os.path.realpath(SCRIPT_DIR)


def _RunCommand(command, working_dir=None, ignore_exit_code=False,
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


def main():
  _RunCommand(['rm', '-rf', os.path.join(CHECKOUT_SRC_DIR, os.pardir, '.cipd')])
  win_toolchain_vsfiles = os.path.join(CHECKOUT_SRC_DIR, 'third_party', 'depot_tools',
                      'win_toolchain', 'vs_files')
  if os.path.exists(win_toolchain_vsfiles):
    _RunCommand(['fusermount', '-u', win_toolchain_vsfiles])
  _RunCommand(['rm', '-rf', os.path.join(CHECKOUT_SRC_DIR, 'third_party')])


if __name__ == '__main__':
  main()
