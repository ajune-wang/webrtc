#!/usr/bin/env python

import argparse
import os
import distutils.dir_util

def main():
  parser = argparse.ArgumentParser(
      description='Tool for cleaning dependencies for specified platform')
  parser.add_argument('--platform', type=str, required=True,
                      choices=['linux', 'mac', 'android', 'ios', 'win'],
                      help='Target platform')
  args = parser.parse_args()

  script_path = os.path.dirname(os.path.abspath(__file__))
  dest_dir = os.path.join(script_path, 'third_party')
  source_dir = os.path.join(script_path, 'third_party_' + args.platform)
  cipd_cache_dir = os.path.join(script_path, os.pardir, '.cipd')

  print("Removing old dependencies set from %s" % dest_dir)
  distutils.dir_util.remove_tree(dest_dir)
  print("Copying new dependencies from %s to %s" % (source_dir, dest_dir))
  distutils.dir_util.copy_tree(source_dir, dest_dir)
  print("Removing .cipd cache dir located in %s" % cipd_cache_dir)
  if os.path.exists(cipd_cache_dir):
    distutils.dir_util.remove_tree(cipd_cache_dir)
  else:
    print("No cipd directory found")
  print("Final dependencies set is: %s" % os.listdir(dest_dir))


if __name__ == '__main__':
  main()
