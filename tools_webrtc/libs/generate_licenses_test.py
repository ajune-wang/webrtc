#!/usr/bin/env python
# pylint: disable=relative-import,protected-access,unused-argument

#  Copyright 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import os
import sys

SRC = os.path.abspath(
    os.path.join(os.path.dirname((__file__)), os.pardir, os.pardir))
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))

import unittest
import mock

from generate_licenses import LicenseBuilder


class TestLicenseBuilder(unittest.TestCase):

  @staticmethod
  def _FakeRunGN(buildfile_dir, target):
    return """
    {
      "target1": {
        "deps": [
          "//a/b/third_party/libname1:c",
          "//a/b/third_party/libname2:c(//d/e/f:g)",
          "//a/b/third_party/libname3/c:d(//e/f/g:h)",
          "//a/b/not_third_party/c"
        ]
      }
    }
    """

  def testParseLibraryName(self):
    self.assertEquals(
        LicenseBuilder._ParseLibraryName('//a/b/third_party/libname1:c'),
        'libname1')
    self.assertEquals(
        LicenseBuilder._ParseLibraryName('//a/b/third_party/libname2:c(d)'),
        'libname2')
    self.assertEquals(
        LicenseBuilder._ParseLibraryName('//a/b/third_party/libname3/c:d(e)'),
        'libname3')
    self.assertEquals(
        LicenseBuilder._ParseLibraryName('//a/b/not_third_party/c'), None)

  def testParseLibrarySimpleMatch(self):
    licenses_dict = {
        'libname': ['path/to/LICENSE'],
    }
    builder = LicenseBuilder([], [], licenses_dict)
    self.assertEquals(
        builder._ParseLibrary('//a/b/third_party/libname:c'), 'libname')

  def testParseLibraryRegExNoMatchFallbacksToDefaultLibname(self):
    licenses_dict = {
        'libname:foo.*': ['path/to/LICENSE'],
    }
    builder = LicenseBuilder([], [], licenses_dict)
    self.assertEquals(
        builder._ParseLibrary('//a/b/third_party/libname:bar_java'), 'libname')

  def testParseLibraryRegExMatch(self):
    licenses_dict = {
        'libname:foo.*': ['path/to/LICENSE'],
    }
    builder = LicenseBuilder([], [], licenses_dict)
    self.assertEquals(
        builder._ParseLibrary('//a/b/third_party/libname:foo_bar_java'),
        'libname:foo.*')

  def testParseLibraryRegExMatchWithSubDirectory(self):
    licenses_dict = {
        'libname/foo:bar.*': ['path/to/LICENSE'],
    }
    builder = LicenseBuilder([], [], licenses_dict)
    self.assertEquals(
        builder._ParseLibrary('//a/b/third_party/libname/foo:bar_java'),
        'libname/foo:bar.*')

  def testGetLibrariesWithRegEx(self):
    licenses_dict = {
        'simple_library': ['simple_license'],
        'library:with_regex': ['license1a'],
        'library:with_second_regex': ['license1b'],
        'another_library/with_another_regex': ['license2'],
    }
    expected_licenses_dict = {
        'library': {'library:with_regex', 'library:with_second_regex'},
        'another_library': {'another_library/with_another_regex'},
    }
    self.assertEquals(
        LicenseBuilder._GetLibrariesWithRegEx(licenses_dict),
        expected_licenses_dict)

  @mock.patch('generate_licenses.LicenseBuilder._RunGN', _FakeRunGN)
  def testGetThirdPartyLibrariesWithoutRegex(self):
    builder = LicenseBuilder([], [], {})
    self.assertEquals(
        builder._GetThirdPartyLibraries('out/arm', 'target1'),
        set(['libname1', 'libname2', 'libname3']))

  @mock.patch('generate_licenses.LicenseBuilder._RunGN', _FakeRunGN)
  def testGetThirdPartyLibrariesWithRegex(self):
    licenses_dict = {
        'libname2:c.*': ['path/to/LICENSE'],
    }
    builder = LicenseBuilder([], [], licenses_dict)
    self.assertEquals(
        builder._GetThirdPartyLibraries('out/arm', 'target1'),
        set(['libname1', 'libname2:c.*', 'libname3']))

  @mock.patch('generate_licenses.LicenseBuilder._RunGN', _FakeRunGN)
  def testGenerateLicenseTextFailIfUnknownLibrary(self):
    licenses_dict = {
        'simple_library': ['simple_license'],
    }
    builder = LicenseBuilder(['dummy_dir'], ['dummy_target'], licenses_dict)

    with self.assertRaises(Exception) as context:
      builder.GenerateLicenseText('dummy/dir')

    self.assertEquals(
        context.exception.message,
        'Missing licenses for following third_party targets: '
        'libname1, libname2, libname3')


if __name__ == '__main__':
  unittest.main()
