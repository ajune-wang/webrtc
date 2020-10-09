/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/scalability_structure_full_svc.h"

#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
enum : int { kKey, kDelta };
}  // namespace

constexpr int ScalabilityStructureFullSvc::kMaxNumSpatialLayers;
constexpr int ScalabilityStructureFullSvc::kMaxNumTemporalLayers;

ScalabilityStructureFullSvc::ScalabilityStructureFullSvc(
    int num_spatial_layers,
    int num_temporal_layers)
    : num_spatial_layers_(num_spatial_layers),
      num_temporal_layers_(num_temporal_layers),
      active_decode_targets_(
          (uint32_t{1} << (num_spatial_layers * num_temporal_layers)) - 1) {
  RTC_DCHECK_LE(num_spatial_layers, kMaxNumSpatialLayers);
  RTC_DCHECK_LE(num_temporal_layers, kMaxNumTemporalLayers);
}

ScalabilityStructureFullSvc::~ScalabilityStructureFullSvc() = default;

ScalabilityStructureFullSvc::StreamLayersConfig
ScalabilityStructureFullSvc::StreamConfig() const {
  StreamLayersConfig result;
  result.num_spatial_layers = num_spatial_layers_;
  result.num_temporal_layers = num_temporal_layers_;
  result.scaling_factor_num[num_spatial_layers_ - 1] = 1;
  result.scaling_factor_den[num_spatial_layers_ - 1] = 1;
  for (int sid = num_spatial_layers_ - 1; sid > 0; --sid) {
    result.scaling_factor_num[sid - 1] = 1;
    result.scaling_factor_den[sid - 1] = 2 * result.scaling_factor_den[sid];
  }
  return result;
}

bool ScalabilityStructureFullSvc::TemporalLayerIsActive(int tid) const {
  if (tid >= num_temporal_layers_) {
    return false;
  }
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    if (DecodeTargetIsActive(sid, tid)) {
      return true;
    }
  }
  return false;
}

DecodeTargetIndication ScalabilityStructureFullSvc::Dti(
    int sid,
    int tid,
    const LayerFrameConfig& config) {
  if (sid < config.SpatialId() || tid < config.TemporalId()) {
    return DecodeTargetIndication::kNotPresent;
  }
  if (sid == config.SpatialId()) {
    if (tid == 0) {
      RTC_DCHECK_EQ(config.TemporalId(), 0);
      return DecodeTargetIndication::kSwitch;
    }
    if (tid == config.TemporalId()) {
      return DecodeTargetIndication::kDiscardable;
    }
    if (tid > config.TemporalId()) {
      RTC_DCHECK_GT(tid, config.TemporalId());
      return DecodeTargetIndication::kSwitch;
    }
  }
  RTC_DCHECK_GT(sid, config.SpatialId());
  RTC_DCHECK_GE(tid, config.TemporalId());
  if (config.IsKeyframe() || config.Id() == kKey) {
    return DecodeTargetIndication::kSwitch;
  }
  return DecodeTargetIndication::kRequired;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureFullSvc::T0FrameConfig(bool is_keyframe) {
  absl::optional<int> spatial_dependency_buffer_id;
  // Disallow temporal references cross T0 on higher temporal layers.
  can_reference_t1_frame_for_spatial_id_.reset();

  std::vector<LayerFrameConfig> configs;
  configs.reserve(num_spatial_layers_);
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    if (!DecodeTargetIsActive(sid, /*tid=*/0)) {
      // Next frame from the spatial layer `sid` shouldn't depend on
      // potentially old previous frame from the spatial layer `sid`.
      can_reference_t0_frame_for_spatial_id_.reset(sid);
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back();
    config.Id(is_keyframe ? kKey : kDelta).S(sid).T(0);

    if (spatial_dependency_buffer_id) {
      config.Reference(*spatial_dependency_buffer_id);
    } else if (is_keyframe) {
      config.Keyframe();
    }

    if (can_reference_t0_frame_for_spatial_id_[sid]) {
      config.ReferenceAndUpdate(BufferIndex(sid, /*tid=*/0));
    } else {
      // TODO(bugs.webrtc.org/11999): Propagate chain restart on delta frame
      // to ChainDiffCalculator
      config.Update(BufferIndex(sid, /*tid=*/0));
    }

    can_reference_t0_frame_for_spatial_id_.set(sid);
    spatial_dependency_buffer_id = BufferIndex(sid, /*tid=*/0);
  }
  RTC_DCHECK(!configs.empty());
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureFullSvc::TXFrameConfig(int tid, int reference_tid) {
  RTC_DCHECK_LT(reference_tid, tid);
  absl::optional<int> spatial_dependency_buffer_id;

  std::vector<LayerFrameConfig> configs;
  configs.reserve(num_spatial_layers_);
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    if (!DecodeTargetIsActive(sid, tid) ||
        !can_reference_t0_frame_for_spatial_id_[sid]) {
      continue;
    }
    configs.emplace_back();
    ScalableVideoController::LayerFrameConfig& config = configs.back();
    config.Id(kDelta).S(sid).T(tid);
    // Temporal reference.
    if (reference_tid == 1 && can_reference_t1_frame_for_spatial_id_[sid]) {
      config.Reference(BufferIndex(sid, /*tid=*/1));
    } else {
      config.Reference(BufferIndex(sid, /*tid=*/0));
    }
    // Spatial reference unless this is the lowest active spatial layer.
    if (spatial_dependency_buffer_id) {
      config.Reference(*spatial_dependency_buffer_id);
    }
    // No frame reference top layer frame, so no need save it into a buffer.
    if (sid < num_spatial_layers_ - 1 || tid < num_temporal_layers_ - 1) {
      config.Update(BufferIndex(sid, tid));
      if (tid == 1) {
        can_reference_t1_frame_for_spatial_id_.set(sid);
      }
    }
    spatial_dependency_buffer_id = BufferIndex(sid, tid);
  }

  if (configs.empty()) {
    static constexpr absl::string_view kFramePatternNames[] = {
        "None", "DeltaT2A", "DeltaT1", "DeltaT2B", "DeltaT0"};
    RTC_LOG(LS_WARNING) << "Failed to generate configuration for L"
                        << num_spatial_layers_ << "T" << num_temporal_layers_
                        << " with active decode targets "
                        << active_decode_targets_.to_string('-').substr(
                               active_decode_targets_.size() -
                               num_spatial_layers_ * num_temporal_layers_)
                        << " and transition to "
                        << kFramePatternNames[last_pattern_] << ". Resetting.";
    return NextFrameConfig(/*restart=*/true);
  }
  return configs;
}

std::vector<ScalableVideoController::LayerFrameConfig>
ScalabilityStructureFullSvc::NextFrameConfig(bool restart) {
  if (active_decode_targets_.none()) {
    last_pattern_ = kNone;
    return {};
  }

  if (restart || last_pattern_ == kNone) {
    can_reference_t0_frame_for_spatial_id_.reset();
    last_pattern_ = kDeltaT0;
    return T0FrameConfig(/*is_keyframe=*/true);
  }

  switch (last_pattern_) {
    case kNone:
      RTC_NOTREACHED();
      return {};
    case kDeltaT2B:
      last_pattern_ = kDeltaT0;
      return T0FrameConfig(/*is_keyframe=*/false);
    case kDeltaT2A:
      if (TemporalLayerIsActive(1)) {
        last_pattern_ = kDeltaT1;
        return TXFrameConfig(/*tid=*/1, /*reference_tid=*/0);
      }
      last_pattern_ = kDeltaT0;
      return T0FrameConfig(/*is_keyframe=*/false);
    case kDeltaT1:
      if (TemporalLayerIsActive(2)) {
        last_pattern_ = kDeltaT2B;
        return TXFrameConfig(/*tid=*/2, /*reference_tid=*/1);
      }
      last_pattern_ = kDeltaT0;
      return T0FrameConfig(/*is_keyframe=*/false);
    case kDeltaT0:
      if (TemporalLayerIsActive(2)) {
        last_pattern_ = kDeltaT2A;
        return TXFrameConfig(/*tid=*/2, /*reference_tid=*/0);
      }
      if (TemporalLayerIsActive(1)) {
        last_pattern_ = kDeltaT1;
        return TXFrameConfig(/*tid=*/1, /*reference_tid=*/0);
      }
      last_pattern_ = kDeltaT0;
      return T0FrameConfig(/*is_keyframe=*/false);
  }
}

absl::optional<GenericFrameInfo> ScalabilityStructureFullSvc::OnEncodeDone(
    LayerFrameConfig config) {
  absl::optional<GenericFrameInfo> frame_info(absl::in_place);
  frame_info->spatial_id = config.SpatialId();
  frame_info->temporal_id = config.TemporalId();
  frame_info->encoder_buffers = config.Buffers();
  frame_info->decode_target_indications.reserve(num_spatial_layers_ *
                                                num_temporal_layers_);
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    for (int tid = 0; tid < num_temporal_layers_; ++tid) {
      frame_info->decode_target_indications.push_back(Dti(sid, tid, config));
    }
  }
  if (config.TemporalId() == 0) {
    frame_info->part_of_chain.resize(num_spatial_layers_);
    for (int sid = 0; sid < num_spatial_layers_; ++sid) {
      frame_info->part_of_chain[sid] = config.SpatialId() <= sid;
    }
  } else {
    frame_info->part_of_chain.assign(num_spatial_layers_, false);
  }
  frame_info->active_decode_targets = active_decode_targets_;
  return frame_info;
}

void ScalabilityStructureFullSvc::OnRatesUpdated(
    const VideoBitrateAllocation& bitrates) {
  for (int sid = 0; sid < num_spatial_layers_; ++sid) {
    // Enable/disable spatial layers independetely.
    bool active = true;
    for (int tid = 0; tid < num_temporal_layers_; ++tid) {
      // To enable temporal layer, require bitrates for lower temporal layers.
      active = active && bitrates.GetBitrate(sid, tid) > 0;
      SetDecodeTargetIsActive(sid, tid, active);
    }
  }
}

}  // namespace webrtc
