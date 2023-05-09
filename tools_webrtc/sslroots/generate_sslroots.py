#!/usr/bin/env vpython3

# -*- coding:utf-8 -*-
# Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
import argparse
import logging
from pathlib import Path
from enum import Enum
from typing import Tuple, Any, List, ByteString
from asn1crypto import pem, x509

_GENERATED_FILE = 'ssl_roots.h'
_MOZILLA_BUNDLE_CHECK = '## Certificate data from Mozilla as of:'


class BundleType(Enum):
  MOZILLA = 'Mozilla'
  GOOGLE = 'Google'


def main():
  parser = argparse.ArgumentParser(
      description='This is a tool to transform a crt file '
      f'into a C/C++ header: {_GENERATED_FILE}.')

  parser.add_argument('pem_file',
                      help='Certificate file path. '
                      'The supported cert files are: '
                      '- Google: https://pki.goog/roots.pem; '
                      '- Mozilla: https://curl.se/docs/caextract.html')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbose',
                      action='store_true',
                      help='Print output while running')
  parser.add_argument('-f',
                      '--full_cert',
                      dest='full_cert',
                      action='store_true',
                      help='Add public key and certificate name. '
                      'Default is to skip and reduce generated file size.')
  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARNING)

  pem_file = Path(args.pem_file).resolve()
  bundle, certificates = _LoadCertificates(pem_file)
  _GenerateCHeader(pem_file.parent / _GENERATED_FILE, bundle, certificates,
                   args.full_cert)


def _LoadCertificates(
    source_file: Path) -> Tuple[BundleType, List[x509.Certificate]]:
  pem_bytes = source_file.read_bytes()
  lines = pem_bytes.decode('utf-8').splitlines()
  bundle = BundleType.MOZILLA if any(
      l.startswith(_MOZILLA_BUNDLE_CHECK) for l in lines) else BundleType.GOOGLE

  certificates = [
      x509.Certificate.load(der)
      for type, _, der in pem.unarmor(pem_bytes, True) if type == 'CERTIFICATE'
  ]
  logging.debug('Loaded %d certificates from %s of %s bundle ',
                len(certificates), source_file, bundle)
  return bundle, certificates


def _GenerateCHeader(header_file: Path, bundle_type: BundleType,
                     certificates: List[x509.Certificate], full_cert: bool):
  header_file.parent.mkdir(parents=True, exist_ok=True)
  with header_file.open('w') as output:
    output.write(_CreateOutputHeader(bundle_type))

    named_certificates = [(cert,
                           f'kCertificateWithFingerprint_{cert.sha256.hex()}')
                          for cert in certificates]

    names = list(map(lambda x: x[1], named_certificates))
    unique_names = list(set(names))
    if len(names) != len(unique_names):
      raise RuntimeError(
          f'There are {len(names) - len(unique_names)} non-unique '
          'certificate names generated. Generator script must be '
          'fixed to handle collision.')

    for cert, safe_name in named_certificates:

      output.write(_CreateCertificateMetadataHeader(cert))

      if full_cert:
        output.write(
            _CArrayConstantDefinition('unsigned char',
                                      f'{safe_name}_subject_name',
                                      _CreateHexList(cert.subject.dump()),
                                      max_items_per_line=16))
        output.write('\n')
        output.write(
            _CArrayConstantDefinition('unsigned char',
                                      f'{safe_name}_public_key',
                                      _CreateHexList(cert.public_key.dump()),
                                      max_items_per_line=16))
        output.write('\n')

      output.write(
          _CArrayConstantDefinition('unsigned char',
                                    f'{safe_name}_certificate',
                                    _CreateHexList(cert.dump()),
                                    max_items_per_line=16))
      output.write('\n\n')

    output.write(
        _CArrayConstantDefinition('unsigned char* const',
                                  'kSSLCertSubjectNameList',
                                  [f'{name}_subject_name' for name in names]))
    output.write('\n\n')

    output.write(
        _CArrayConstantDefinition('unsigned char* const',
                                  'kSSLCertPublicKeyList',
                                  [f'{name}_public_key' for name in names]))
    output.write('\n\n')

    output.write(
        _CArrayConstantDefinition('unsigned char* const',
                                  'kSSLCertCertificateList',
                                  [f'{name}_certificate' for name in names]))
    output.write('\n\n')

    output.write(
        _CArrayConstantDefinition(
            'size_t', 'kSSLCertCertificateSizeList',
            [f'{len(cert.dump())}' for cert, _ in named_certificates]))
    output.write('\n\n')

    output.write(_CreateOutputFooter())


def _CreateHexList(items: ByteString) -> List[str]:
  """
  Produces list of strings each item is hex literal of byte of source sequence
  """
  return [f'0x{item:02X}' for item in items]


def _CArrayConstantDefinition(type_name: str,
                              array_name: str,
                              items: List[Any],
                              max_items_per_line: int = 1) -> str:
  """
  Produces C array definition like: `const type_name array_name = { items };`
  """
  return (f'const {type_name} {array_name}[{len(items)}]='
          f'{_CArrayInitializerList(items, max_items_per_line)};')


def _CArrayInitializerList(items: List[Any],
                           max_items_per_line: int = 1) -> str:
  """
  Produces C initializer list like: `{\\nitems[0], \\n ...}`
  """
  return '{\n' + '\n'.join([
      ','.join(items[i:i + max_items_per_line]) + ','
      for i in range(0, len(items), max_items_per_line)
  ]) + '\n}'


def _CreateCertificateMetadataHeader(cert: x509.Certificate) -> str:
  return (f'/* subject: {cert.subject.human_friendly} */\n'
          f'/* issuer: {cert.issuer.human_friendly} */\n'
          f'/* link: https://crt.sh/?q={cert.sha256.hex()} */\n')


def _CreateOutputHeader(bundle_type: BundleType) -> str:
  output = ('/*\n'
            ' *  Copyright 2023 The WebRTC Project Authors. All rights '
            'reserved.\n'
            ' *\n'
            ' *  Use of this source code is governed by a BSD-style license\n'
            ' *  that can be found in the LICENSE file in the root of the '
            'source\n'
            ' *  tree. An additional intellectual property rights grant can be '
            'found\n'
            ' *  in the file PATENTS.  All contributing project authors may\n'
            ' *  be found in the AUTHORS file in the root of the source tree.\n'
            ' */\n\n'
            '#ifndef RTC_BASE_SSL_ROOTS_H_\n'
            '#define RTC_BASE_SSL_ROOTS_H_\n\n'
            '// This file is the root certificates in C form.\n\n'
            '// It was generated with the following script:\n'
            '// tools_webrtc/sslroots/generate_sslroots.py'
            f' {bundle_type.value}_CA_bundle.pem\n\n'
            '// clang-format off\n'
            '// Don\'t bother formatting generated code,\n'
            '// also it would breaks subject/issuer lines.\n\n')
  return output


def _CreateOutputFooter():
  return '// clang-format on\n\n#endif  // RTC_BASE_SSL_ROOTS_H_\n'


if __name__ == '__main__':
  main()
