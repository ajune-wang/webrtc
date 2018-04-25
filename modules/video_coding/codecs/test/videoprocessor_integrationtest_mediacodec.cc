/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

#include <vector>

#include "api/test/create_videoprocessor_integrationtest_fixture.h"
#include "common_types.h"  // NOLINT(build/include)
#include "media/base/mediaconstants.h"
#include "test/field_trial.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kForemanNumFrames = 300;
}  // namespace

class VideoProcessorIntegrationTestMediaCodec : public ::testing::Test {
 protected:
  VideoProcessorIntegrationTestMediaCodec() {
    fixture_ = CreateVideoProcessorIntegrationTestFixture();
    fixture_->config.filename = "foreman_cif";
    fixture_->config.filepath = ResourcePath(fixture_->config.filename, "yuv");
    fixture_->config.num_frames = kForemanNumFrames;
    fixture_->config.hw_encoder = true;
    fixture_->config.hw_decoder = true;
  }
  std::unique_ptr<VideoProcessorIntegrationTestFixtureInterface> fixture_;
};

TEST_F(VideoProcessorIntegrationTestMediaCodec, ForemanCif500kbpsVp8) {
  fixture_->config.SetCodecSettings(cricket::kVp8CodecName, 1, 1, 1, false,
                                    false, false, false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  fixture_->ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                                        &quality_thresholds, nullptr, nullptr);
}

TEST_F(VideoProcessorIntegrationTestMediaCodec, ForemanCif500kbpsH264CBP) {
  const auto frame_checker =
      rtc::MakeUnique<VideoProcessorIntegrationTest::H264KeyframeChecker>();
  fixture_->config.encoded_frame_checker = frame_checker.get();
  fixture_->config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false,
                                    false, false, false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {10, 1, 1, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 31, 0.92, 0.86}};

  fixture_->ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                                        &quality_thresholds, nullptr, nullptr);
}

// TODO(brandtr): Enable this test when we have trybots/buildbots with
// HW encoders that support CHP.
TEST_F(VideoProcessorIntegrationTestMediaCodec,
       DISABLED_ForemanCif500kbpsH264CHP) {
  ScopedFieldTrials override_field_trials("WebRTC-H264HighProfile/Enabled/");
  const auto frame_checker =
      rtc::MakeUnique<VideoProcessorIntegrationTest::H264KeyframeChecker>();

  fixture_->config.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  fixture_->config.encoded_frame_checker = frame_checker.get();
  fixture_->config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false,
                                    false, false, false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  // The thresholds below may have to be tweaked to let even poor MediaCodec
  // implementations pass. If this test fails on the bots, disable it and
  // ping brandtr@.
  std::vector<RateControlThresholds> rc_thresholds = {
      {5, 1, 0, 0.1, 0.2, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 35, 0.93, 0.91}};

  fixture_->ProcessFramesAndMaybeVerify(rate_profiles, &rc_thresholds,
                                        &quality_thresholds, nullptr, nullptr);
}

}  // namespace test
}  // namespace webrtc
