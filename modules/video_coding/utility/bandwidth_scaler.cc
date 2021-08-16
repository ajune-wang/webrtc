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

#include "api/video/video_adaptation_reason.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/weak_ptr.h"
#include "vector"

namespace webrtc {

BitRateOveruseOptions::BitRateOveruseOptions() {
  high_encode_usage_threshold_percent = 110;
  low_encode_usage_threshold_percent = 20;
  low_use_consequent_threshold_count = 2;
  high_use_consequent_threshold_count = 2;
}

BandwidthScaler::BandwidthScaler(BandwidthScalerUsageHandlerInterface* handler)
    : handler_(handler),
      low_use_consequent_count(0),
      high_use_consequent_count(0),
      average_encode_size(kDefaultAverageEncodeSize),
      last_sample_ms_(0),
      last_check_bitrate_ms_(rtc::TimeMillis()),
      tot_encode_count_during_one_cycle(0) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  StartCheckForBitrate();
  RTC_DCHECK(handler_ != nullptr);
}

absl::optional<VideoEncoder::ResolutionBitrateLimits>
BandwidthScaler::GetBitRateLimitedForResolution(int width_, int height_) {
  const std::vector<VideoEncoder::ResolutionBitrateLimits>& limits =
      GetTestBitrateLimits();

  for (size_t i = 0; i < limits.size(); ++i) {
    if (limits[i].frame_size_pixels >= width_ * height_) {
      return limits[i];
    }
  }
  return absl::nullopt;
}

void BandwidthScaler::StartCheckForBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  TaskQueueBase::Current()->PostDelayedTask(
      ToQueuedTask([this, weak_handler_ = handler_] {
        switch (CheckBitrate()) {
          case BandwidthScaler::CheckBitrateResult::kHighBitRate: {
            weak_handler_->OnReportUsageBandwidthHigh();
            ClearDataAfterCheckResultIsValid();
            ClearSampleData();
            break;
          }
          case BandwidthScaler::CheckBitrateResult::kLowBitRate: {
            weak_handler_->OnReportUsageBandwidthLow();
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
          default:
            break;
        }
        tot_encode_count_during_one_cycle = 0;
        StartCheckForBitrate();
      }),
      GetCheckingBitRateDelayMs());
}

float BandwidthScaler::GetCheckingBitRateDelayMs() {
  return CheckingBitRateDelayMs;
}

void BandwidthScaler::ReportEncodeInfo(float encode_frame_size,
                                       int64_t time_sent_in_us,
                                       uint32_t _encodedWidth,
                                       uint32_t _encodedHeight) {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (frame_info.has_value()) {
    if (frame_info.value()._encodedHeight_ != _encodedHeight ||
        frame_info.value()._encodedWidth_ != _encodedWidth) {
      average_encode_size.Reset(kDefaultAverageEncodeSize);
    }
    frame_info.value()._encodedHeight_ = _encodedHeight;
    frame_info.value()._encodedWidth_ = _encodedWidth;
  } else {
    frame_info = FrameInfo(_encodedWidth, _encodedHeight);
  }
  AddToExpFileter(encode_frame_size, time_sent_in_us);
}

BandwidthScaler::CheckBitrateResult BandwidthScaler::CheckBitrate() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  if (!frame_info.has_value()) {
    RTC_LOG(LS_INFO) << "BandwidthScaler::CheckBitrateResult frame_info is "
                        "null,because there is no data in filter";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  float now_time_ms = rtc::TimeMillis();
  // Usually,the gap_time_ms is about 5000ms.In other words,it is almost
  // CheckingBitRateDelayMs.Sometime maybe 5005 or 4992
  float gap_time_ms = now_time_ms - last_check_bitrate_ms_;
  last_check_bitrate_ms_ = now_time_ms;

  // Multiplying by eight is to convert bytes to bits
  float now_average_bitrate = average_encode_size.filtered() *
                              tot_encode_count_during_one_cycle * 8 /
                              (gap_time_ms / 1000);

  absl::optional<VideoEncoder::ResolutionBitrateLimits> suitable_limits =
      GetBitRateLimitedForResolution(frame_info.value()._encodedWidth_,
                                     frame_info.value()._encodedHeight_);

  if (!suitable_limits.has_value()) {
    RTC_LOG(LS_INFO) << "BandwidthScaler::CheckBitrateResult suitable_limits "
                        "is null,the resolution is not in vector range";
    return BandwidthScaler::CheckBitrateResult::kInsufficientSamples;
  }

  if (now_average_bitrate > suitable_limits.value().max_bitrate_bps) {
    ++low_use_consequent_count;
    if (low_use_consequent_count >=
        Options.low_use_consequent_threshold_count) {
      return BandwidthScaler::CheckBitrateResult::kLowBitRate;
    }
  } else if (now_average_bitrate < suitable_limits.value().min_bitrate_bps) {
    ++high_use_consequent_count;
    if (high_use_consequent_count >=
        Options.high_use_consequent_threshold_count) {
      return BandwidthScaler::CheckBitrateResult::kHighBitRate;
    }
  } else {
    return BandwidthScaler::CheckBitrateResult::kNormalBitrate;
  }
  return BandwidthScaler::CheckBitrateResult::kNeedDoubleCheckBitRate;
}

void BandwidthScaler::ClearDataAfterCheckResultIsValid() {
  high_use_consequent_count = low_use_consequent_count = 0;
}

void BandwidthScaler::AddToExpFileter(float sample, int64_t time_sent_us) {
  int64_t now_ms = time_sent_us / 1000;
  average_encode_size.Apply(static_cast<float>(now_ms - last_sample_ms_),
                            sample);
  ++tot_encode_count_during_one_cycle;
  last_sample_ms_ = now_ms;
}

void BandwidthScaler::ClearSampleData() {
  RTC_DCHECK_RUN_ON(&task_checker_);
  average_encode_size.Reset(kDefaultAverageEncodeSize);
  frame_info.reset();
}

BandwidthScalerUsageHandlerInterface::~BandwidthScalerUsageHandlerInterface() {}
}  // namespace webrtc
