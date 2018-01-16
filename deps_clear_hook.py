#!/usr/bin/env python

import argparse
import os
import shutil

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
    'syzygy',
    'winsdk_samples',
  ],
  'android': [
    'accessibility_test_framework',
    'android_platform',
    'android_support_test_runner',
    'apk-patch-size-estimator',
    'ashmem',
    'auto',
    'bazel',
    'bouncycastle',
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
    'android_ndk',
    'android_system_sdk',
    'android_tools',
  ],
  'mac': [
    'ocmock'
  ],
  'ios': [
    'ocmock'
  ],
}


def main():
  parser = argparse.ArgumentParser(
      description='Tool for cleaning dependencies for specified platform')
  parser.add_argument('--platform', type=str, required=True,
                      choices=PLATFORM_DEPS.keys(),
                      help='Target platform')
  parser.add_argument('--dir', type=str,
                      help='Third party directory to clean')
  parser.add_argument('--verbose', type=bool, default=False,
                      help='Print more detailed output')
  args = parser.parse_args()

  third_party_dir = os.path.join(os.curdir, args.dir)

  valid_dependencies = set(COMMON_DEPS + PLATFORM_DEPS[args.platform])
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
    print "Current dependencies: %s" % cur_dependencies


if __name__ == '__main__':
  main()
