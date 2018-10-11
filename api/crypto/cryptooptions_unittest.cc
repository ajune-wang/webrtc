/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/crypto/cryptooptions.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

// Validates the copy constructor correctly copies data.
TEST(CryptoOptionsTest, TestCopyConstructor) {
  CryptoOptions lhs;
  lhs.srtp.enable_gcm_crypto_suites = true;
  lhs.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  lhs.srtp.enable_encrypted_rtp_header_extensions = true;
  lhs.frame.require_frame_encryption = true;

  CryptoOptions rhs = lhs;
  EXPECT_EQ(lhs.srtp.enable_gcm_crypto_suites,
            rhs.srtp.enable_gcm_crypto_suites);
  EXPECT_EQ(lhs.srtp.enable_aes128_sha1_32_crypto_cipher,
            rhs.srtp.enable_aes128_sha1_32_crypto_cipher);
  EXPECT_EQ(lhs.srtp.enable_encrypted_rtp_header_extensions,
            rhs.srtp.enable_encrypted_rtp_header_extensions);
  EXPECT_EQ(lhs.frame.require_frame_encryption,
            rhs.frame.require_frame_encryption);
}

// Validates the overloaded equality operator functions correctly.
TEST(CryptoOptionsTest, Equality) {
  CryptoOptions lhs;
  CryptoOptions rhs;
  EXPECT_EQ(lhs, rhs);

  lhs.srtp.enable_gcm_crypto_suites = true;
  EXPECT_NE(lhs, rhs);
  rhs.srtp.enable_gcm_crypto_suites = true;
  EXPECT_EQ(lhs, rhs);

  lhs.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  EXPECT_NE(lhs, rhs);
  rhs.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  EXPECT_EQ(lhs, rhs);

  lhs.srtp.enable_encrypted_rtp_header_extensions = true;
  EXPECT_NE(lhs, rhs);
  rhs.srtp.enable_encrypted_rtp_header_extensions = true;
  EXPECT_EQ(lhs, rhs);

  lhs.frame.require_frame_encryption = true;
  EXPECT_NE(lhs, rhs);
  rhs.frame.require_frame_encryption = true;
  EXPECT_EQ(lhs, rhs);
}

}  // namespace
}  // namespace webrtc
