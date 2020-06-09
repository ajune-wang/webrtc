/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "absl/strings/match.h"
#include "api/test/create_videocodec_test_fixture.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t3.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

class RTC_EXPORT LibaomSvcEncoderFactory : public VideoEncoderFactory {
 public:
  LibaomSvcEncoderFactory(
      std::unique_ptr<ScalableVideoController> svc_controller)
      : svc_controller_(std::move(svc_controller)) {}

  static std::vector<SdpVideoFormat> SupportedFormats();
  std::vector<SdpVideoFormat> GetSupportedFormats() const override;

  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;

 private:
  std::unique_ptr<ScalableVideoController> svc_controller_;
};

std::vector<SdpVideoFormat> LibaomSvcEncoderFactory::SupportedFormats() {
  std::vector<SdpVideoFormat> supported_codecs;
  if (kIsLibaomAv1EncoderSupported)
    supported_codecs.push_back(SdpVideoFormat(cricket::kAv1CodecName));
  return supported_codecs;
}

std::vector<SdpVideoFormat> LibaomSvcEncoderFactory::GetSupportedFormats()
    const {
  return SupportedFormats();
}

VideoEncoderFactory::CodecInfo LibaomSvcEncoderFactory::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  CodecInfo info;
  info.is_hardware_accelerated = false;
  info.has_internal_source = false;
  return info;
}

std::unique_ptr<VideoEncoder> LibaomSvcEncoderFactory::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  if (kIsLibaomAv1EncoderSupported &&
      absl::EqualsIgnoreCase(format.name, cricket::kAv1CodecName))
    return CreateLibaomAv1Encoder(std::move(svc_controller_));
  RTC_LOG(LS_ERROR) << "Trying to created encoder of unsupported format "
                    << format.name;
  return nullptr;
}

// Test clips settings.
constexpr int kCifWidth = 352;
constexpr int kCifHeight = 288;
constexpr int kNumFramesLong = 300;

VideoCodecTestFixture::Config CreateConfig(std::string filename) {
  VideoCodecTestFixture::Config config;
  config.filename = filename;
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.use_single_core = true;
  return config;
}

TEST(VideoCodecTestLibaom, HighBitrateAV1) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {12, 1, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 34, 0.94, 0.92}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestLibaom, VeryLowBitrateAV1) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{50, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {15, 8, 75, 2, 2, 2, 2, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{28, 25, 0.70, 0.62}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

#if !defined(WEBRTC_ANDROID)
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
TEST(VideoCodecTestLibaom, HdAV1) {
  auto config = CreateConfig("ConferenceMotion_1280_720_50");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kHdWidth, kHdHeight);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{1000, 50, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {13, 3, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 32, 0.93, 0.87}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST(VideoCodecTestLibaom, HdSvc3SL3TL) {
  std::unique_ptr<ScalableVideoController> svc_controller =
      std::make_unique<ScalabilityStructureL3T3>();
  std::unique_ptr<VideoEncoderFactory> encoder_factory =
      std::make_unique<LibaomSvcEncoderFactory>(std::move(svc_controller));
  std::unique_ptr<VideoDecoderFactory> decoder_factory =
      std::make_unique<InternalDecoderFactory>();

  auto config = CreateConfig("ConferenceMotion_1280_720_50");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 3, 3, false, true, true,
                          kHdWidth, kHdHeight);

  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config, std::move(decoder_factory),
                                             std::move(encoder_factory));

  std::vector<RateProfile> rate_profiles = {{1000, 50, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {13, 3, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{36, 32, 0.93, 0.87}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}
#endif

}  // namespace
}  // namespace test
}  // namespace webrtc
