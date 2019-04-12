/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "crypto/hkdf.h"

#include <utility>

#include "test/gmock.h"

namespace webrtc {
namespace {

// Validates that a basic valid call works correctly.
TEST(Hkdf, DerivationBasicTest) {
  rtc::Buffer secret(32);
  rtc::Buffer salt(32);
  rtc::Buffer label(32);
  const size_t derived_key_byte_size = 16;

  auto key_or = HkdfSha256(secret, salt, label, derived_key_byte_size);
  EXPECT_TRUE(key_or.has_value());
  rtc::ZeroOnFreeBuffer<uint8_t> key = std::move(key_or.value());
  EXPECT_EQ(derived_key_byte_size, key.size());
}

// Derivation fails if output is too small.
TEST(Hkdf, DerivationFailsIfOutputIsTooSmall) {
  rtc::Buffer secret(32);
  rtc::Buffer salt(32);
  rtc::Buffer label(32);
  const size_t derived_key_byte_size = 15;

  auto key_or = HkdfSha256(secret, salt, label, derived_key_byte_size);
  EXPECT_FALSE(key_or.has_value());
}

// Derivation fails if output is too large.
TEST(Hkdf, DerivationFailsIfOutputIsTooLarge) {
  rtc::Buffer secret(32);
  rtc::Buffer salt(32);
  rtc::Buffer label(32);
  const size_t derived_key_byte_size = 256 * 32;

  auto key_or = HkdfSha256(secret, salt, label, derived_key_byte_size);
  EXPECT_FALSE(key_or.has_value());
}

// Validates that too little key material causes a failure.
TEST(Hkdf, DerivationFailsWithInvalidSecret) {
  rtc::Buffer secret(15);
  rtc::Buffer salt(32);
  rtc::Buffer label(32);
  const size_t derived_key_byte_size = 16;

  auto key_or_0 = HkdfSha256(secret, salt, label, derived_key_byte_size);
  EXPECT_FALSE(key_or_0.has_value());

  auto key_or_1 = HkdfSha256(nullptr, salt, label, derived_key_byte_size);
  EXPECT_FALSE(key_or_1.has_value());

  rtc::Buffer secret_empty;
  auto key_or_2 = HkdfSha256(secret_empty, salt, label, derived_key_byte_size);
  EXPECT_FALSE(key_or_2.has_value());
}

// Validates that HKDF works without a salt being set.
TEST(Hkdf, DerivationWorksWithNoSalt) {
  rtc::Buffer secret(32);
  rtc::Buffer label(32);
  const size_t derived_key_byte_size = 16;

  auto key_or = HkdfSha256(secret, nullptr, label, derived_key_byte_size);
  EXPECT_TRUE(key_or.has_value());
}

// Validates that a label is required to work correctly.
TEST(Hkdf, DerivationRequiresLabel) {
  rtc::Buffer secret(32);
  rtc::Buffer salt(32);
  rtc::Buffer label(1);
  const size_t derived_key_byte_size = 16;

  auto key_or_0 = HkdfSha256(secret, salt, label, derived_key_byte_size);
  EXPECT_TRUE(key_or_0.has_value());
  rtc::ZeroOnFreeBuffer<uint8_t> key = std::move(key_or_0.value());
  EXPECT_EQ(key.size(), derived_key_byte_size);

  auto key_or_1 = HkdfSha256(secret, salt, nullptr, derived_key_byte_size);
  EXPECT_FALSE(key_or_1.has_value());
}

}  // namespace
}  // namespace webrtc
