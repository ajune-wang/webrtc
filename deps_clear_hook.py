#!/usr/bin/env python

import argparse
import os
import shutil
import distutils.dir_util

COMMON_DEPS = [
  'binutils',
  'boringssl',
  'ced',
   'depot_tools',
   'ffmpeg',
  'freetype',
  'googletest',
  'harfbuzz-ng',
   'icu',
  'instrumented_libraries',
  'jsoncpp',
  'libFuzzer',
   'libjpeg_turbo',
  'libpng',
   'libsrtp',
  'libvpx',
   'libyuv',
   'llvm-build',
   'lss',
  'mockito',
  'openh264',
   'openmax_dl',
  'opus',
  'protobuf',
  'requests',
  'usrsctp',
  'yasm',
  'zlib',

   'catapult',
  'colorama',
   'gtest-parallel',
]

PLATFORM_DEPS = {
  'linux': [],
  'win': [
    # externally sync deps
     'syzygy',
     'winsdk_samples',
  ],
  'android': [
    # compile time deps
    'accessibility_test_framework',
    'android_platform',
    'android_support_test_runner',
    'apk-patch-size-estimator',
    'ashmem',
    'auto',
    'bazel',
    'bouncycastle',
    'breakpad',
    'byte_buddy',
    'closure_compiler',
    'errorprone',
    'espresso',
    'eu-strip',
    'gson',
    'guava',
    'hamcrest',
    'icu4j',
    'ijar',
    'intellij',
    'javax_inject',
    'jinja2',
    'jsr-305',
    'junit',
    'libxml',
    'markupsafe',
    'modp_b64',
    'objenesis',
    'ow2_asm',
    'robolectric',
    'sqlite4java',
    'tcmalloc',
    'ub-uiautomator',
    'xstream',
    # test time deps
    'proguard',

    # externally sync deps
     'android_ndk',
    'android_system_sdk',
     'android_tools',
     'findbugs',
  ],
  'mac': [
    'ocmock'
  ],
  'ios': [
    'ocmock'
  ],
}


def get_valid_deps_list(required_platforms):
  valid_dependencies = COMMON_DEPS
  for platform, deps in PLATFORM_DEPS.iteritems():
    if platform in required_platforms or 'all' in required_platforms:
      valid_dependencies.extend(deps)
  return valid_dependencies


def main():
  parser = argparse.ArgumentParser(
      description='Tool for cleaning dependencies for specified platform')
  parser.add_argument('--platform', default=[], action='append',
                      choices=PLATFORM_DEPS.keys() + ['all'],
                      help='Target platform')
  parser.add_argument('--dir', type=str,
                      help='Third party directory to clean')
  parser.add_argument('--verbose', action='store_true', default=False,
                      help='Print more detailed output')
  args = parser.parse_args()

  script_path = os.path.dirname(os.path.abspath(__file__))
  if args.dir is not None:
    third_party_dir = os.path.join(os.curdir, args.dir)
  else:
    third_party_dir = os.path.join(script_path, 'third_party')

  required_platforms = set(args.platform)
  if args.verbose:
    print 'Clearing deps for platforms: %s' % required_platforms
  valid_dependencies = set(get_valid_deps_list(required_platforms))
  cur_dependencies = os.listdir(third_party_dir)
  if args.verbose:
    print 'Found %s dependencies' % len(cur_dependencies)
  for dependency in cur_dependencies:
    if dependency in valid_dependencies:
      continue
    dependency_path = os.path.join(third_party_dir, dependency)
    if not os.path.isdir(dependency_path):
      continue
    if args.verbose:
      print 'Removing dependency at %s' % dependency_path
    # We failed to remove WebKit on Windows because of bad file naming, so lets skip it
    if args.platform == 'win' and dependency == 'WebKit':
      print 'Skipping WebKit for Windows'
      continue
    shutil.rmtree(dependency_path)

  if args.verbose:
    cur_dependencies = os.listdir(third_party_dir)
    print "Current dependencies (%s): %s" % (
      len(cur_dependencies), cur_dependencies)


if __name__ == '__main__':
  main()
