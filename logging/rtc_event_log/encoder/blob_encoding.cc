/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/blob_encoding.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr size_t kMaxVarIntLengthBytes = 10;  // ceil(64 / 7.0) is 10.

// TODO: !!! Document
std::string EncodeVarInt(uint64_t input) {
  std::string output;
  output.reserve(kMaxVarIntLengthBytes);

  do {
    uint8_t byte = static_cast<uint8_t>(input & 0x7f);
    input >>= 7;
    if (input > 0) {
      byte |= 0x80;
    }
    output.insert(output.end(), byte);
  } while (input > 0);

  RTC_DCHECK_GE(output.size(), 1u);
  RTC_DCHECK_LE(output.size(), kMaxVarIntLengthBytes);

  return output;
}
}  // namespace

std::string EncodeBlobs(const std::vector<std::string>& blobs) {
  RTC_DCHECK(!blobs.empty());

  size_t result_length_bound = kMaxVarIntLengthBytes * blobs.size();
  std::for_each(blobs.begin(), blobs.end(),
                [&result_length_bound](const std::string& str) {
                  result_length_bound += str.length();
                });

  std::string result;
  result.reserve(result_length_bound);

  for (const std::string& blob : blobs) {
    result += EncodeVarInt(blob.length());
    result += blob;
  }

  return result;
}

std::vector<std::string> DecodeBlobs(const std::string& encoded_blobs,
                                     size_t num_of_blobs) {
  RTC_DCHECK(!encoded_blobs.empty());
  return std::vector<std::string>();
}

}  // namespace webrtc
