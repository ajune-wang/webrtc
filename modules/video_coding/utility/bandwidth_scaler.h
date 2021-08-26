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
#include "rtc_base/rate_statistics.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class BandwidthScalerUsageHandlerInterface {
 public:
  virtual ~BandwidthScalerUsageHandlerInterface();

  virtual void OnReportUsageBandwidthHigh() = 0;
  virtual void OnReportUsageBandwidthLow() = 0;
};

// BandwidthScaler runs asynchronously and monitors bandwidth values of encoded
// frames. It holds a reference to a BandwidthScalerUsageHandlerInterface
// implementation to signal an overuse or underuse of bandwidth (which indicate
// a desire to scale the video stream down or up).
class BandwidthScaler {
 public:
  explicit BandwidthScaler(BandwidthScalerUsageHandlerInterface* handler);
  virtual ~BandwidthScaler();

  void ReportEncodeInfo(int frame_size,
                        int64_t time_sent_in_ms,
                        uint32_t encoded_width,
                        uint32_t encoded_height);

  static absl::optional<VideoEncoder::ResolutionBitrateLimits>
  GetBitrateLimitsForResolution(int width, int height);

  const TimeDelta kBitrateStateUpdateInterval;

 private:
  enum class CheckBitrateResult {
    kInsufficientSamples,
    kNormalBitrate,
    kHighBitRate,
    kLowBitRate,
  };

  struct FrameInfo {
    FrameInfo(uint32_t encoded_width, uint32_t encoded_height)
        : encoded_width(encoded_width), encoded_height(encoded_height) {}
    uint32_t encoded_width;
    uint32_t encoded_height;
  };

  // We will periodically check encodebitrate, this function will make
  // adjustment decisions and report the decision to the adapter.
  void StartCheckForBitrate();
  CheckBitrateResult CheckBitrate();
  bool IsOverUsing();
  bool IsUnderUsing();

  RTC_NO_UNIQUE_ADDRESS SequenceChecker task_checker_;
  BandwidthScalerUsageHandlerInterface* const handler_
      RTC_GUARDED_BY(&task_checker_);

  RateStatistics average_encode_rate_ RTC_GUARDED_BY(&task_checker_);
  absl::optional<FrameInfo> frame_info_ RTC_GUARDED_BY(&task_checker_);
  rtc::WeakPtrFactory<BandwidthScaler> weak_ptr_factory_;
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CODING_UTILITY_BANDWIDTH_SCALER_H_
