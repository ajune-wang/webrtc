/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/frame_dependencies_controller_full_svc.h"

#include <utility>

#include "api/transport/rtp/dependency_descriptor.h"
#include "api/video/video_frame_type.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "rtc_base/checks.h"

namespace webrtc {

FrameDependencyStructure
FrameDependenciesControllerFullSvc::DependencyStructure() const {
  FrameDependencyStructure structure;
  structure.num_decode_targets = max_spatial_layers_ * max_temporal_layers_;
  structure.num_chains = 0;
  if (max_resolution_) {
    // TODO(danilchap): Support other factors than 2 of reducing resolution.
    // 1.5 scale in particular that is used in some predefined av1 modes.
    for (int sid = 0; sid < max_spatial_layers_; ++sid) {
      structure.resolutions.emplace_back(
          max_resolution_->Width() >> (max_spatial_layers_ - 1 - sid),
          max_resolution_->Height() >> (max_spatial_layers_ - 1 - sid));
    }
  }
  // TODO(danilchap): Generate/set more useful templates.
  structure.templates.reserve(max_spatial_layers_ * max_temporal_layers_);
  for (int sid = 0; sid < max_spatial_layers_; ++sid) {
    for (int tid = 0; tid < max_temporal_layers_; ++tid) {
      FrameDependencyTemplate a_template;
      a_template.spatial_id = sid;
      a_template.temporal_id = tid;
      for (int dti_sid = 0; dti_sid < max_spatial_layers_; ++dti_sid) {
        for (int dti_tid = 0; dti_tid < max_temporal_layers_; ++dti_tid) {
          a_template.decode_target_indications.push_back(
              sid <= dti_sid && tid <= dti_tid
                  ? DecodeTargetIndication::kRequired
                  : DecodeTargetIndication::kNotPresent);
        }
      }
      structure.templates.push_back(std::move(a_template));
    }
  }

  return structure;
}

std::vector<GenericFrameInfo>
FrameDependenciesControllerFullSvc::NextFrameConfig(bool reset) {
  if (reset) {
    temporal_unit_template_idx_ = 0;
  }
  std::vector<GenericFrameInfo> result(max_spatial_layers_);
  // TODO(danilchap): Support other number of layers too.
  RTC_DCHECK_EQ(max_spatial_layers_, 3);
  RTC_DCHECK_EQ(max_temporal_layers_, 3);
  switch (temporal_unit_template_idx_) {
    case 0:  // Key frame
    case 1:  // Delta frame T0
      result[0] =
          GenericFrameInfo::Builder().S(0).T(0).Dtis("SSSSSSSSS").Build();
      result[0].encoder_buffers = {CodecBufferUsage(
          /*id=*/0, /*references=*/temporal_unit_template_idx_ == 1,
          /*updates=*/true)};

      result[1] =
          GenericFrameInfo::Builder().S(1).T(0).Dtis("---SSSSSS").Build();
      result[1].encoder_buffers = {
          CodecBufferUsage(/*id=*/0, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/1, /*references=*/false, /*updates=*/true)};

      result[2] =
          GenericFrameInfo::Builder().S(2).T(0).Dtis("------SSS").Build();
      result[2].encoder_buffers = {
          CodecBufferUsage(/*id=*/1, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/2, /*references=*/false, /*updates=*/true)};

      temporal_unit_template_idx_ = 2;
      break;
    case 2:  // Delta frame first T2
      result[0] =
          GenericFrameInfo::Builder().S(0).T(2).Dtis("--D--R--R").Build();
      result[0].encoder_buffers = {
          CodecBufferUsage(/*id=*/0, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/3, /*references=*/false, /*updates=*/true)};

      result[1] =
          GenericFrameInfo::Builder().S(1).T(2).Dtis("-----D--R").Build();
      result[1].encoder_buffers = {
          CodecBufferUsage(/*id=*/1, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/3, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/4, /*references=*/false, /*updates=*/true)};

      result[2] =
          GenericFrameInfo::Builder().S(2).T(2).Dtis("--------D").Build();
      result[2].encoder_buffers = {
          CodecBufferUsage(/*id=*/2, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/4, /*references=*/true, /*updates=*/false)};

      temporal_unit_template_idx_ = 3;
      break;
    case 3:  // Delta frame T1
      result[0] =
          GenericFrameInfo::Builder().S(0).T(1).Dtis("-DS-RR-RR").Build();
      result[0].encoder_buffers = {
          CodecBufferUsage(/*id=*/0, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/5, /*references=*/false, /*updates=*/true)};

      result[1] =
          GenericFrameInfo::Builder().S(1).T(1).Dtis("----DR-RR").Build();
      result[1].encoder_buffers = {
          CodecBufferUsage(/*id=*/1, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/5, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/6, /*references=*/false, /*updates=*/true)};

      result[2] =
          GenericFrameInfo::Builder().S(2).T(1).Dtis("-------DS").Build();
      result[2].encoder_buffers = {
          CodecBufferUsage(/*id=*/2, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/6, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/7, /*references=*/false, /*updates=*/true)};

      temporal_unit_template_idx_ = 4;
      break;
    case 4:  // Delta frame second T2
      result[0] =
          GenericFrameInfo::Builder().S(0).T(2).Dtis("--D--R--R").Build();
      result[0].encoder_buffers = {
          CodecBufferUsage(/*id=*/5, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/3, /*references=*/false, /*updates=*/true)};

      result[1] =
          GenericFrameInfo::Builder().S(1).T(2).Dtis("-----D--R").Build();
      result[1].encoder_buffers = {
          CodecBufferUsage(/*id=*/3, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/4, /*references=*/false, /*updates=*/true)};

      result[2] =
          GenericFrameInfo::Builder().S(2).T(2).Dtis("--------D").Build();
      result[2].encoder_buffers = {
          CodecBufferUsage(/*id=*/4, /*references=*/true, /*updates=*/false),
          CodecBufferUsage(/*id=*/7, /*references=*/true, /*updates=*/false)};

      temporal_unit_template_idx_ = 1;
      break;
    default:
      RTC_CHECK(false);
      break;
  }
  return result;
}

}  // namespace webrtc
