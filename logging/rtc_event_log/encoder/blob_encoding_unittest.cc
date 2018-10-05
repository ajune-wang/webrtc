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

#include <string>
#include <vector>

#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void TestEncodingAndDecoding(const std::vector<absl::string_view>& blobs) {
  RTC_DCHECK(!blobs.empty());

  const std::string encoded = EncodeBlobs(blobs);
  ASSERT_FALSE(encoded.empty());

  const std::vector<absl::string_view> decoded =
      DecodeBlobs(encoded, blobs.size());
  EXPECT_EQ(decoded, blobs);
}

}  // namespace

TEST(BlobEncoding, EmptyBlob) {
  TestEncodingAndDecoding({""});
}

TEST(BlobEncoding, SingleCharacterBlob) {
  TestEncodingAndDecoding({"a"});
}

// TODO: !!!
TEST(BlobEncoding, LongBlob) {
  // std::vector<std::string> blobs = {""};
}

}  // namespace webrtc
