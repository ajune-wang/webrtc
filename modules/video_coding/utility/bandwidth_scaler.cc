/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/bandwidth_scaler.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

namespace {

// Return the suitable bitrate limit intreval for specified
// resolution,they are experimental values.
// TODO(shuhai): Maybe we need to add other codecs(VP8/VP9) experimental
// values.
std::vector<VideoEncoder::ResolutionBitrateLimits>
GetDefaultResolutionBitrateLimits() {
  // Specific limits for H264/AVC
  return {{320 * 180, 0, 0, 300000},
          {480 * 270, 200000, 300000, 500000},
          {640 * 360, 300000, 500000, 800000},
          {960 * 540, 500000, 800000, 1500000},
          {1280 * 720, 900000, 1500000, 2500000}};
}

constexpr float kDefaultAverageEncodeSizeAlpha = 0.995f;
constexpr TimeDelta kBitrateStateUpdateInterval = TimeDelta::Seconds(5);

}  // namespace

BitRateOveruseOptions::BitRateOveruseOptions() {
  high_encode_usage_threshold_percent = 110;
  low_encode_usage_threshold_percent = 20;
  low_use_consequent_threshold_count = 2;
  high_use_consequent_threshold_count = 2;
}

BandwidthScaler::BandwidthScaler(BandwidthScalerUsageHandlerInterface* handler)
    : handler_(handler),
      low_use_consequent_count_(0),
      high_use_consequent_count_(0),
      average_encode_bps_(kDefaultAverageEncodeSizeAlpha),
      last_sample_ms_(0) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  StartCheckForBitrate();
  RTC_DCHECK(handler_ != nullptr);
}

absl::optional<VideoEncoder::ResolutionBitrateLimits>
BandwidthScaler::GetBitRateLimitedForResolution(int width, int height) {
  const std::vector<VideoEncoder::ResolutionBitrateLimits>& limits =
      GetDefaultResolutionBitrateLimits();

  for (size_t i = 0; i < limits.size(); ++i) {
    if (limits[i].frame_size_pixels >= width * height) {
      return limits[i];
    }
  }
  return absl::nullopt;
}

void BandwidthScaler::StartCheckForBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  TaskQueueBase::Current()->PostDelayedTask(
      ToQueuedTask([this] {
        RTC_DCHECK_RUN_ON(&task_checker_);
        switch (CheckBitrate()) {
          case BandwidthScaler::CheckBitrateResult::kHighBitRate: {
            handler_->OnReportUsageBandwidthHigh();
            ClearDataAfterCheckResultIsValid();
            ClearSampleData();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kLowBitRate: {
            handler_->OnReportUsageBandwidthLow();
            ClearDataAfterCheckResultIsValid();
            ClearSampleData();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kNormalBitrate: {
            ClearDataAfterCheckResultIsValid();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kInsufficientSamples:
          case BandwidthScaler::CheckBitrateResult::kNeedDoubleCheckBitRate: {
            break;
          }
        }
        StartCheckForBitrate();
      }),
      kBitrateStateUpdateInterval.ms());
}

void BandwidthScaler::ReportEncodeInfo(float frame_contribute_bps,
                                       int64_t time_sent_in_us,
                                       uint32_t encoded_width,
                                       uint32_t encoded_height) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (frame_info.has_value()) {
    if (frame_info->encoded_height_ != encoded_height ||
        frame_info->encoded_width_ != encoded_width) {
      average_encode_bps_.Reset(kDefaultAverageEncodeSizeAlpha);
      frame_info->encoded_height_ = encoded_height;
      frame_info->encoded_width_ = encoded_width;
    }
  } else {
    frame_info.emplace(encoded_width, encoded_height);
  }
  AddToExpFileter(frame_contribute_bps, time_sent_in_us);
}

BandwidthScaler::CheckBitrateResult BandwidthScaler::CheckBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (!frame_info.has_value()) {
    RTC_LOG(LS_INFO) << "BandwidthScaler::CheckBitrateResult frame_info is "
                        "null,because there is no data in filter";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  float now_average_bitrate = average_encode_bps_.filtered();

  absl::optional<VideoEncoder::ResolutionBitrateLimits> suitable_limits =
      GetBitRateLimitedForResolution(frame_info->encoded_width_,
                                     frame_info->encoded_height_);

  if (!suitable_limits.has_value()) {
    RTC_LOG(LS_INFO) << "BandwidthScaler::CheckBitrateResult suitable_limits "
                        "is null,the resolution is not in vector range";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  if (now_average_bitrate > suitable_limits.value().max_bitrate_bps) {
    ++low_use_consequent_count_;
    if (low_use_consequent_count_ >=
        options_.low_use_consequent_threshold_count) {
      return BandwidthScaler::CheckBitrateResult::kLowBitRate;
    }
  } else if (now_average_bitrate < suitable_limits.value().min_bitrate_bps) {
    ++high_use_consequent_count_;
    if (high_use_consequent_count_ >=
        options_.high_use_consequent_threshold_count) {
      return BandwidthScaler::CheckBitrateResult::kHighBitRate;
    }
  } else {
    return BandwidthScaler::CheckBitrateResult::kNormalBitrate;
  }
  return BandwidthScaler::CheckBitrateResult::kNeedDoubleCheckBitRate;
}

void BandwidthScaler::ClearDataAfterCheckResultIsValid() {
  high_use_consequent_count_ = low_use_consequent_count_ = 0;
}

void BandwidthScaler::AddToExpFileter(float sample, int64_t time_sent_us) {
  int64_t now_ms = time_sent_us / 1000;
  average_encode_bps_.Apply(static_cast<float>(now_ms - last_sample_ms_),
                            sample);
  last_sample_ms_ = now_ms;
}

void BandwidthScaler::ClearSampleData() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  average_encode_bps_.Reset(kDefaultAverageEncodeSizeAlpha);
  frame_info.reset();
}

BandwidthScalerUsageHandlerInterface::~BandwidthScalerUsageHandlerInterface() {}
}  // namespace webrtc
