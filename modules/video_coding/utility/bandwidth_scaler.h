/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_BANDWIDTH_SCALER_H_
#define MODULES_VIDEO_CODING_UTILITY_BANDWIDTH_SCALER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/numerics/moving_average.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

struct BitRateOveruseOptions {
  BitRateOveruseOptions();

  int low_encode_usage_threshold_percent;  // Threshold for triggering underuse.
  int high_encode_usage_threshold_percent;  // Threshold for triggering overuse.
  int low_use_consequent_threshold_count;  // Threshold times for the consequent
                                           // underuse
  int high_use_consequent_threshold_count;  // Threshold times for the
                                            // consequent overuse
};

struct FrameInfo {
  FrameInfo(uint32_t _encodedWidth, uint32_t _encodedHeight) {
    _encodedWidth_ = _encodedWidth;
    _encodedHeight_ = _encodedHeight;
  }
  uint32_t _encodedWidth_;
  uint32_t _encodedHeight_;
};

const float kDefaultAverageEncodeSize = 0.9f;
const float CheckingBitRateDelayMs = 5000;

class BandwidthScalerUsageHandlerInterface;

// BandwidthScaler runs asynchronously and monitors bandwidth values of encoded
// frames. It holds a reference to a BandwidthScalerUsageHandlerInterface
// implementation to signal an overuse or underuse of bandwidth (which indicate
// a desire to scale the video stream down or up).
class BandwidthScaler {
 public:
  explicit BandwidthScaler(BandwidthScalerUsageHandlerInterface* handler);

  void ReportEncodeInfo(float encode_frame_size,
                        int64_t time_sent_in_us,
                        uint32_t _encodedWidth,
                        uint32_t _encodedHeight);

  absl::optional<VideoEncoder::ResolutionBitrateLimits>
  GetBitRateLimitedForResolution(int width_, int height_);

 private:
  enum class CheckBitrateResult {
    kInsufficientSamples,
    kNormalBitrate,
    kHighBitRate,
    kLowBitRate,
    kNeedDoubleCheckBitRate,
  };

  void StartCheckForBitrate();
  CheckBitrateResult CheckBitrate();
  bool IsOverUsing();
  bool IsUnderUsing();
  float GetCheckingBitRateDelayMs();

  void AddToExpFileter(float sample, int64_t time_sent_us);
  void ClearDataAfterCheckResultIsValid();
  void ClearSampleData();

  // Return the suitable bitrate limit intreval for specified
  // resolution,they are experimental values.
  // TODO(shuhai): Maybe we need to add other codecs(VP8/VP9) experimental
  // values.
  std::vector<VideoEncoder::ResolutionBitrateLimits> GetTestBitrateLimits() {
    // Specific limits for H264/AVC
    return {{320 * 180, 0, 0, 300000},
            {480 * 270, 200000, 300000, 500000},
            {640 * 360, 300000, 500000, 800000},
            {960 * 540, 500000, 800000, 1500000},
            {1280 * 720, 900000, 1500000, 2500000}};
  }

  RTC_NO_UNIQUE_ADDRESS SequenceChecker task_checker_;

  BandwidthScalerUsageHandlerInterface* handler_ RTC_GUARDED_BY(&task_checker_);

  int low_use_consequent_count;
  int high_use_consequent_count;
  BitRateOveruseOptions Options;
  rtc::ExpFilter average_encode_size;
  int64_t last_sample_ms_;
  float last_check_bitrate_ms_;
  int tot_encode_count_during_one_cycle;
  absl::optional<FrameInfo> frame_info RTC_GUARDED_BY(&task_checker_);
};

class BandwidthScalerUsageHandlerInterface {
 public:
  virtual ~BandwidthScalerUsageHandlerInterface();

  virtual void OnReportUsageBandwidthHigh() = 0;
  virtual void OnReportUsageBandwidthLow() = 0;
};
}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_UTILITY_BANDWIDTH_SCALER_H_
