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

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "api/video/video_adaptation_reason.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/bandwidth_scaler_settings.h"
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
  return {{0 * 0, 0, 0, 0},
          {320 * 180, 0, 0, 300000},
          {480 * 270, 200000, 300000, 500000},
          {640 * 360, 300000, 500000, 800000},
          {960 * 540, 500000, 800000, 1500000},
          {1280 * 720, 900000, 1500000, 2500000},
          {1920 * 1080, 1500000, 2500000, 4000000}};
}

constexpr int kDefaultMaxWindowSizeMs = 5000;

// The linear interpolation implementation(x1<=x<=x2).
static int linearInterpolation(int x1, int y1, int x2, int y2, int x) {
  if (x1 == x2)
    return (y1 + y2) / 2;
  const float alpha = 1.0 * (x - x1) / (x2 - x1);
  return (y2 - y1) * alpha + y1;
}

}  // namespace

BandwidthScaler::BandwidthScaler(BandwidthScalerUsageHandlerInterface* handler)
    : kBitrateStateUpdateInterval(
          TimeDelta::Seconds(BandwidthScalerSettings::ParseFromFieldTrials()
                                 .BitrateStateUpdateInterval()
                                 .value_or(5))),
      handler_(handler),
      average_encode_rate_(kDefaultMaxWindowSizeMs, RateStatistics::kBpsScale),
      weak_ptr_factory_(this) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  RTC_DCHECK(handler_ != nullptr);

  StartCheckForBitrate();
}

BandwidthScaler::~BandwidthScaler() {
  RTC_DCHECK_RUN_ON(&task_checker_);
}

absl::optional<VideoEncoder::ResolutionBitrateLimits>
BandwidthScaler::GetBitRateLimitedForResolution(int width, int height) {
  if (width <= 0 || height <= 0) {
    return absl::nullopt;
  }

  const std::vector<VideoEncoder::ResolutionBitrateLimits>& limits =
      GetDefaultResolutionBitrateLimits();

  if (limits.size() == 0) {
    return absl::nullopt;
  }

  int interpolation_index = -1;
  for (size_t i = 0; i < limits.size(); ++i) {
    if (limits[i].frame_size_pixels >= width * height) {
      interpolation_index = i;
      break;
    }
  }

  // -1 means that the maximum resolution is exceeded, we will select the
  // largest data as the return result.
  if (interpolation_index == -1) {
    return *limits.rbegin();
  } else {
    if (limits[interpolation_index].frame_size_pixels == width * height) {
      return limits[interpolation_index];
    } else {
      int interpolation_min_bps = linearInterpolation(
          limits[interpolation_index - 1].frame_size_pixels,
          limits[interpolation_index - 1].min_bitrate_bps,
          limits[interpolation_index].frame_size_pixels,
          limits[interpolation_index].min_bitrate_bps, width * height);

      int interpolation_max_bps = linearInterpolation(
          limits[interpolation_index - 1].frame_size_pixels,
          limits[interpolation_index - 1].max_bitrate_bps,
          limits[interpolation_index].frame_size_pixels,
          limits[interpolation_index].max_bitrate_bps, width * height);

      RTC_DCHECK_GE(interpolation_max_bps, interpolation_min_bps);
      if (interpolation_max_bps >= interpolation_min_bps) {
        return VideoEncoder::ResolutionBitrateLimits(
            width * height, interpolation_min_bps, interpolation_min_bps,
            interpolation_max_bps);
      } else {
        RTC_LOG(LS_WARNING)
            << "BitRate interpolation calculating result is abnormal.";
        return absl::nullopt;
      }
    }
  }
}

void BandwidthScaler::StartCheckForBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  TaskQueueBase::Current()->PostDelayedTask(
      ToQueuedTask([this_weak_ptr = weak_ptr_factory_.GetWeakPtr(), this] {
        if (!this_weak_ptr) {
          // The caller BandwidthScaler has been deleted.
          return;
        }
        RTC_DCHECK_RUN_ON(&task_checker_);
        switch (CheckBitrate()) {
          case BandwidthScaler::CheckBitrateResult::kHighBitRate: {
            handler_->OnReportUsageBandwidthHigh();
            frame_info_.reset();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kLowBitRate: {
            handler_->OnReportUsageBandwidthLow();
            frame_info_.reset();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kNormalBitrate: {
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kInsufficientSamples: {
            break;
          }
        }

        StartCheckForBitrate();
      }),
      kBitrateStateUpdateInterval.ms());
}

void BandwidthScaler::ReportEncodeInfo(int frame_size,
                                       int64_t time_sent_in_ms,
                                       uint32_t encoded_width,
                                       uint32_t encoded_height) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (frame_info_.has_value()) {
    if (frame_info_->encoded_height != encoded_height ||
        frame_info_->encoded_width != encoded_width) {
      frame_info_->encoded_height = encoded_height;
      frame_info_->encoded_width = encoded_width;
    }
  } else {
    frame_info_.emplace(encoded_width, encoded_height);
  }
  average_encode_rate_.Update(frame_size, time_sent_in_ms);
}

BandwidthScaler::CheckBitrateResult BandwidthScaler::CheckBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (!frame_info_.has_value()) {
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  absl::optional<int64_t> now_average_bitrate =
      average_encode_rate_.Rate(rtc::TimeMillis());
  if (!now_average_bitrate.has_value()) {
    RTC_LOG(LS_INFO) << "Get bitrate sliding averaging window rate failed.";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  absl::optional<VideoEncoder::ResolutionBitrateLimits> suitable_bitrate_limit =
      GetBitRateLimitedForResolution(frame_info_->encoded_width,
                                     frame_info_->encoded_height);

  if (!suitable_bitrate_limit.has_value()) {
    RTC_LOG(LS_INFO) << "Get suitable_bitrate_limit failed.";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  if (now_average_bitrate > suitable_bitrate_limit->max_bitrate_bps) {
    return BandwidthScaler::CheckBitrateResult::kLowBitRate;
  } else if (now_average_bitrate < suitable_bitrate_limit->min_bitrate_bps) {
    return BandwidthScaler::CheckBitrateResult::kHighBitRate;
  }
  return BandwidthScaler::CheckBitrateResult::kNormalBitrate;
}

BandwidthScalerUsageHandlerInterface::~BandwidthScalerUsageHandlerInterface() {}
}  // namespace webrtc
