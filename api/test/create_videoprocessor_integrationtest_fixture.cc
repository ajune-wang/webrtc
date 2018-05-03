/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "api/test/create_videoprocessor_integrationtest_fixture.h"
#include "api/test/videoprocessor_integrationtest_fixture.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/codecs/test/videoprocessor_integrationtest.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {
namespace test {

std::unique_ptr<VideoProcessorIntegrationTestFixtureInterface>
CreateVideoProcessorIntegrationTestFixture(TestConfig config) {
  return rtc::MakeUnique<VideoProcessorIntegrationTest>(config);
}

std::unique_ptr<VideoProcessorIntegrationTestFixtureInterface>
CreateVideoProcessorIntegrationTestFixture(
    TestConfig config,
    std::unique_ptr<VideoDecoderFactory> decoder_factory,
    std::unique_ptr<VideoEncoderFactory> encoder_factory) {
  return rtc::MakeUnique<VideoProcessorIntegrationTest>(
      config, std::move(decoder_factory), std::move(encoder_factory));
}

}  // namespace test
}  // namespace webrtc
