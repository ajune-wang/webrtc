/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalable_video_controller_l2t2.h"

#include <utility>
#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr auto kNotPresent = DecodeTargetIndication::kNotPresent;
constexpr auto kDiscardable = DecodeTargetIndication::kDiscardable;
constexpr auto kSwitch = DecodeTargetIndication::kSwitch;
constexpr auto kRequired = DecodeTargetIndication::kRequired;

constexpr DecodeTargetIndication kDtis[6][4] = {
    {kSwitch, kSwitch, kSwitch, kSwitch},                   // KeyFrame S0
    {kNotPresent, kNotPresent, kSwitch, kSwitch},           // KeyFrame S1
    {kNotPresent, kDiscardable, kNotPresent, kRequired},    // DeltaFrame S0T1
    {kNotPresent, kNotPresent, kNotPresent, kDiscardable},  // DeltaFrame S1T1
    {kSwitch, kSwitch, kSwitch, kSwitch},                   // DeltaFrame S0T0
    {kNotPresent, kNotPresent, kSwitch, kRequired},         // DeltaFrame S1T0
};

}  // namespace

ScalableVideoControllerL2T2::~ScalableVideoControllerL2T2() = default;

ScalableVideoController::StreamLayersConfig
ScalableVideoControllerL2T2::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = 2;
  result.num_temporal_layers = 2;
  return result;
}

FrameDependencyStructure ScalableVideoControllerL2T2::DependencyStructure()
    const {
  using Builder = GenericFrameInfo::Builder;
  FrameDependencyStructure structure;
  structure.num_decode_targets = 4;
  structure.num_chains = 2;
  structure.decode_target_protected_by_chain = {0, 1};
  structure.templates = {
      Builder().S(0).T(0).Dtis("SSSS").ChainDiffs({0, 0}).Build(),
      Builder().S(0).T(0).Dtis("SSSS").ChainDiffs({4, 3}).Fdiffs({4}).Build(),
      Builder().S(1).T(0).Dtis("--SS").ChainDiffs({1, 1}).Fdiffs({1}).Build(),
      Builder()
          .S(1)
          .T(0)
          .Dtis("--SR")
          .ChainDiffs({1, 1})
          .Fdiffs({1, 4})
          .Build(),
      Builder().S(0).T(1).Dtis("-D-R").ChainDiffs({2, 1}).Fdiffs({2}).Build(),
      Builder()
          .S(1)
          .T(1)
          .Dtis("---D")
          .ChainDiffs({3, 2})
          .Fdiffs({1, 2})
          .Build(),
  };
  return structure;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalableVideoControllerL2T2::NextFrameConfig(bool restart) {
  if (restart) {
    next_pattern_ = kKeyFrame;
  }
  std::vector<LayerFrameConfig> result(2);

  // Buffer 0 keeps last S0T0 frame,
  // Buffer 1 keeps last S1T0 frame,
  // Buffer 2 keeps last S0T1 frame.
  if (next_pattern_ == kKeyFrame) {
    result[0].id = 0;
    result[0].spatial_id = 0;
    result[0].temporal_id = 0;
    result[0].is_keyframe = true;
    result[0].buffers = {{/*id=*/0, /*references=*/false, /*updates=*/true}};

    result[1].id = 1;
    result[1].spatial_id = 1;
    result[1].temporal_id = 0;
    result[1].is_keyframe = false;
    result[1].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                         {/*id=*/1, /*references=*/false, /*updates=*/true}};
    next_pattern_ = kDeltaFrameT1;
  } else if (next_pattern_ == kDeltaFrameT1) {
    result[0].id = 2;
    result[0].spatial_id = 0;
    result[0].temporal_id = 1;
    result[0].is_keyframe = false;
    result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                         {/*id=*/2, /*references=*/false, /*updates=*/true}};

    result[1].id = 3;
    result[1].spatial_id = 1;
    result[1].temporal_id = 1;
    result[1].is_keyframe = false;
    result[1].buffers = {{/*id=*/2, /*references=*/true, /*updates=*/false},
                         {/*id=*/1, /*references=*/true, /*updates=*/false}};
    next_pattern_ = kDeltaFrameT0;
  } else if (next_pattern_ == kDeltaFrameT0) {
    result[0].id = 4;
    result[0].spatial_id = 0;
    result[0].temporal_id = 0;
    result[0].is_keyframe = false;
    result[0].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/true}};

    result[1].id = 5;
    result[1].spatial_id = 1;
    result[1].temporal_id = 0;
    result[1].is_keyframe = false;
    result[1].buffers = {{/*id=*/0, /*references=*/true, /*updates=*/false},
                         {/*id=*/1, /*references=*/true, /*updates=*/true}};
    next_pattern_ = kDeltaFrameT1;
  } else {
    RTC_NOTREACHED();
  }
  return result;
}

absl::optional<GenericFrameInfo> ScalableVideoControllerL2T2::OnEncodeDone(
    LayerFrameConfig config) {
  if (config.is_keyframe) {
    config.id = 0;
  }

  absl::optional<GenericFrameInfo> frame_info;
  if (config.id < 0 || config.id > 5) {
    RTC_LOG(LS_WARNING) << "Unexpected config id " << config.id;
    return frame_info;
  }
  frame_info.emplace().encoder_buffers = std::move(config.buffers);
  frame_info->decode_target_indications.assign(std::begin(kDtis[config.id]),
                                               std::end(kDtis[config.id]));
  if (config.temporal_id == 0) {
    frame_info->part_of_chain = {config.spatial_id == 0, true};
  } else {
    frame_info->part_of_chain = {false, false};
  }
  return frame_info;
}

}  // namespace webrtc
