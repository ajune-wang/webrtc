/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/opensslcertificate.h"
#include <string>
#include <vector>
#include "rtc_base/gunit.h"
#include "rtc_base/openssl.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sslcertificate.h"
#include "rtc_base/sslroots.h"

namespace rtc {

TEST(OpenSSLBuiltinRootCertLoaderTest, BuiltinReturnsCertificatesIfCompiledTo) {
  auto cert_loader = MakeUnique<OpenSSLBuiltinRootCertLoader>();
  SSLCertChain root_certs = cert_loader->Load();
#ifndef WEBRTC_DISABLE_BUILT_IN_SSL_ROOT_CERTIFICATES
  EXPECT_NE(root_certs.GetSize(), static_cast<size_t>(0));
#else
  EXPECT_EQ(root_certs.GetSize(), static_cast<size_t>(0));
#endif
}

}  // namespace rtc
