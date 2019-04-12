/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CRYPTO_HKDF_H_
#define CRYPTO_HKDF_H_

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"

namespace webrtc {

// Derives a new key from existing key material using HKDF.
// secret - The random secret value you wish to derive a key from.
// salt - Optional (non secret) cryptographically random value.
// label - A non secret but unique label value to determine the derivation.
// derived_key_byte_size - The size of the derived key.
// return - A ZeroOnFreeBuffer containing the derived key or an error
// condition. Checking error codes is explicit in the API and error should
// never be ignored.
absl::optional<rtc::ZeroOnFreeBuffer<uint8_t>> HkdfSha256(
    rtc::ArrayView<const uint8_t> secret,
    rtc::ArrayView<const uint8_t> salt,
    rtc::ArrayView<const uint8_t> label,
    size_t derived_key_byte_size);

}  // namespace webrtc

#endif  // CRYPTO_HKDF_H_
