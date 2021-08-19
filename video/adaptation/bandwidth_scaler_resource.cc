/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/bandwidth_scaler_resource.h"

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

// static
rtc::scoped_refptr<BandwidthScalerResource> BandwidthScalerResource::Create() {
  return rtc::make_ref_counted<BandwidthScalerResource>();
}

BandwidthScalerResource::BandwidthScalerResource()
    : VideoStreamEncoderResource("BandwidthScalerResource"),
      bandwidth_scaler(nullptr) {}

BandwidthScalerResource::~BandwidthScalerResource() {
  RTC_DCHECK(!bandwidth_scaler);
}

bool BandwidthScalerResource::is_started() const {
  RTC_DCHECK_RUN_ON(encoder_queue());
  return bandwidth_scaler.get();
}

void BandwidthScalerResource::StartCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(!is_started());
  bandwidth_scaler = std::make_unique<BandwidthScaler>(this);
}

void BandwidthScalerResource::StopCheckForOveruse() {
  RTC_DCHECK_RUN_ON(encoder_queue());
  RTC_DCHECK(is_started());
  // Ensure we have no pending callbacks. This makes it safe to destroy the
  // BandwidthScaler and even task queues with tasks in-flight.
  bandwidth_scaler.reset();
}

void BandwidthScalerResource::OnReportUsageBandwidthHigh() {
  OnResourceUsageStateMeasured(ResourceUsageState::kOveruse);
}

void BandwidthScalerResource::OnReportUsageBandwidthLow() {
  OnResourceUsageStateMeasured(ResourceUsageState::kUnderuse);
}

void BandwidthScalerResource::OnEncodeCompleted(
    const EncodedImage& encoded_image,
    int64_t time_sent_in_us,
    int64_t encode_image_size) {
  RTC_DCHECK_RUN_ON(encoder_queue());

  if (bandwidth_scaler) {
    if (last_time_encode_start_ms.has_value()) {
      int64_t encode_delta_time_ms = (encoded_image.timing_.encode_start_ms -
                                      last_time_encode_start_ms.value());
      if (encode_delta_time_ms > 0) {
        float frame_contribute_bps =
            last_encode_image_size_ * 8.0 / (encode_delta_time_ms / 1000.0);
        bandwidth_scaler->ReportEncodeInfo(
            frame_contribute_bps, time_sent_in_us, encoded_image._encodedWidth,
            encoded_image._encodedHeight);
      }

      last_encode_image_size_ = encode_image_size;
      last_time_encode_start_ms = encoded_image.timing_.encode_start_ms;
    } else {
      last_encode_image_size_ = encode_image_size;
      last_time_encode_start_ms.emplace(encoded_image.timing_.encode_start_ms);
    }
  }
}

}  // namespace webrtc
