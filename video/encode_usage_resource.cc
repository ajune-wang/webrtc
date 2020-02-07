/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/encode_usage_resource.h"

#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

EncodeUsageResource::EncodeUsageResource(
    std::unique_ptr<OveruseFrameDetector> overuse_detector)
    : overuse_detector_(std::move(overuse_detector)), is_started_(false) {
  RTC_DCHECK(overuse_detector_);
}

void EncodeUsageResource::StartCheckForOveruse(CpuOveruseOptions options) {
  RTC_DCHECK(!is_started_);
  overuse_detector_->StartCheckForOveruse(TaskQueueBase::Current(),
                                          std::move(options), this);
  is_started_ = true;
}

void EncodeUsageResource::StopCheckForOveruse() {
  overuse_detector_->StopCheckForOveruse();
  is_started_ = false;
}

void EncodeUsageResource::OnEncodeCompleted(
    int64_t capture_time_us,
    absl::optional<int> encode_duration_us) {
  // TODO(hbos): Rename FrameSent() to something more appropriate (e.g.
  // "OnEncodeCompleted"?).
  overuse_detector_->FrameSent(capture_time_us, encode_duration_us);
}

void EncodeUsageResource::AdaptUp(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kCpu);
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

bool EncodeUsageResource::AdaptDown(AdaptReason reason) {
  RTC_DCHECK_EQ(reason, AdaptReason::kCpu);
  return OnResourceUsageStateMeasured(ResourceUsageState::kOveruse) !=
         ResourceListenerResponse::kQualityScalerShouldIncreaseFrequency;
}

}  // namespace webrtc
