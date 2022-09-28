/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/decoder_database.h"

#include <memory>

#include "api/test/mock_video_decoder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Test registering and unregistering an external decoder instance.
TEST(VCMDecoderDataBase, RegisterExternalDecoder) {
  VCMDecoderDataBase db;
  constexpr int kPayloadType = 1;
  ASSERT_FALSE(db.IsExternalDecoderRegistered(kPayloadType));

  auto decoder = std::make_unique<testing::StrictMock<MockVideoDecoder>>();
  db.RegisterExternalDecoder(kPayloadType, decoder.get());
  EXPECT_TRUE(db.IsExternalDecoderRegistered(kPayloadType));
  EXPECT_EQ(decoder.get(), db.DeregisterExternalDecoder(kPayloadType));
  EXPECT_FALSE(db.IsExternalDecoderRegistered(kPayloadType));
}

TEST(VCMDecoderDataBase, RegisterReceiveCodec) {
  VCMDecoderDataBase db;
  constexpr int kPayloadType = 1;
  ASSERT_FALSE(db.DeregisterReceiveCodec(kPayloadType));

  VideoDecoder::Settings settings;
  settings.set_codec_type(kVideoCodecVP8);
  settings.set_max_render_resolution({10, 10});
  settings.set_number_of_cores(4);
  db.RegisterReceiveCodec(kPayloadType, settings);

  EXPECT_TRUE(db.DeregisterReceiveCodec(kPayloadType));
}

TEST(VCMDecoderDataBase, DeregisterReceiveCodecs) {
  VCMDecoderDataBase db;
  constexpr int kPayloadType1 = 1, kPayloadType2 = 2;
  ASSERT_FALSE(db.DeregisterReceiveCodec(kPayloadType1));
  ASSERT_FALSE(db.DeregisterReceiveCodec(kPayloadType2));

  VideoDecoder::Settings settings1;
  settings1.set_codec_type(kVideoCodecVP8);
  settings1.set_max_render_resolution({10, 10});
  settings1.set_number_of_cores(4);

  VideoDecoder::Settings settings2 = settings1;
  settings2.set_codec_type(kVideoCodecVP9);

  db.RegisterReceiveCodec(kPayloadType1, settings1);
  db.RegisterReceiveCodec(kPayloadType2, settings2);

  db.DeregisterReceiveCodecs();

  // All receive codecs must have been removed.
  EXPECT_FALSE(db.DeregisterReceiveCodec(kPayloadType1));
  EXPECT_FALSE(db.DeregisterReceiveCodec(kPayloadType2));
}

}  // namespace
}  // namespace webrtc
