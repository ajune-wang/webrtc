/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_CREATE_PEERCONNECTION_QUALITY_TEST_FRAME_GENERATOR_H_
#define API_TEST_CREATE_PEERCONNECTION_QUALITY_TEST_FRAME_GENERATOR_H_

#include <string>

#include "absl/types/optional.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void ValidateScreenShareConfig(const VideoConfig& video_config, const ScreenShareConfig& screen_share_config);

std::unique_ptr<FrameGeneratorInterface> CreateSquareFrameGenerator(const PeerConnectionE2EQualityTestFixture::VideoConfig& config, absl::optional<FrameGeneratorInterface::OutputType> type);

std::unique_ptr<FrameGeneratorInterface> CreateFromYuvFileFrameGenerator(const PeerConnectionE2EQualityTestFixture::VideoConfig& config, std::string filename);

std::unique_ptr<FrameGeneratorInterface> CreateScrollingInputFromYuvFilesFrameGenerator(const PeerConnectionE2EQualityTestFixture::VideoConfig& config, const PeerConnectionE2EQualityTestFixture::ScreenShareConfig& screen_share_config);

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // API_TEST_CREATE_PEERCONNECTION_QUALITY_TEST_FRAME_GENERATOR_H_

