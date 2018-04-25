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
#include "media/base/mediaconstants.h"
#include "modules/video_coding/codecs/test/objc_codec_factory_helper.h"
#include "test/field_trial.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

namespace {
const int kForemanNumFrames = 300;
}  // namespace

class VideoProcessorIntegrationTestVideoToolbox : public testing::Test {
 protected:
  VideoProcessorIntegrationTestVideoToolbox() {
    auto decoder_factory = CreateObjCDecoderFactory();
    auto encoder_factory = CreateObjCEncoderFactory();
    fixture_ = CreateVideoProcessorIntegrationTestFixture(
        std::move(decoder_factory), std::move(encoder_factory));
    frame_checker_ =
        rtc::MakeUnique<VideoProcessorIntegrationTest::H264KeyframeChecker>();
    fixture_->config.filename = "foreman_cif";
    fixture_->config.filepath = ResourcePath(fixture_->config.filename, "yuv");
    fixture_->config.num_frames = kForemanNumFrames;
    fixture_->config.hw_encoder = true;
    fixture_->config.hw_decoder = true;
    fixture_->config.encoded_frame_checker = frame_checker_.get();
  }
  std::unique_ptr<VideoProcessorIntegrationTestFixtureInterface> fixture_;
  std::unique_ptr<TestConfig::EncodedFrameChecker> frame_checker_;
};

// TODO(webrtc:9099): Disabled until the issue is fixed.
// HW codecs don't work on simulators. Only run these tests on device.
// #if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
// #define MAYBE_TEST_F TEST_F
// #else
#define MAYBE_TEST_F(s, name) TEST_F(s, DISABLED_##name)
// #endif

// TODO(kthelgason): Use RC Thresholds when the internal bitrateAdjuster is no
// longer in use.
MAYBE_TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       ForemanCif500kbpsH264CBP) {
  fixture_->config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false,
                                    false, false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 29, 0.9, 0.82}};

  fixture_->ProcessFramesAndMaybeVerify(rate_profiles, nullptr,
                                        &quality_thresholds, nullptr, nullptr);
}

MAYBE_TEST_F(VideoProcessorIntegrationTestVideoToolbox,
       ForemanCif500kbpsH264CHP) {
  ScopedFieldTrials override_field_trials("WebRTC-H264HighProfile/Enabled/");

  fixture_->config.h264_codec_settings.profile = H264::kProfileConstrainedHigh;
  fixture_->config.SetCodecSettings(cricket::kH264CodecName, 1, 1, 1, false,
                                    false, false, 352, 288);

  std::vector<RateProfile> rate_profiles = {{500, 30, kForemanNumFrames}};

  std::vector<QualityThresholds> quality_thresholds = {{33, 30, 0.91, 0.83}};

  fixture_->ProcessFramesAndMaybeVerify(rate_profiles, nullptr,
                                        &quality_thresholds, nullptr, nullptr);
}

}  // namespace test
}  // namespace webrtc
