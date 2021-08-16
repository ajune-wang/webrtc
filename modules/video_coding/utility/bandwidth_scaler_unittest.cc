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
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gtest.h"

namespace webrtc {

static const int kFramerateForTest = 30;

class FakeBandwidthHandler : public BandwidthScalerUsageHandlerInterface {
 public:
  ~FakeBandwidthHandler() override = default;
  void OnReportUsageBandwidthHigh() override {
    adapt_down_events_++;
    event_.Set();
  }

  void OnReportUsageBandwidthLow() override {
    adapt_up_events_++;
    event_.Set();
  }

  rtc::Event event_;
  int adapt_up_events_ = 0;
  int adapt_down_events_ = 0;
};

class BandwidthScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection {
    kKeepScaleNormalBandwidth,
    kKeepScaleAboveMaxBandwidth,
    kKeepScaleUnderMinBandwidth,
  };

  BandwidthScalerTest()
      : task_queue_("BandwidthScalerTestQueue"),
        handler_(std::make_unique<FakeBandwidthHandler>()) {
    task_queue_.SendTask(
        [this] {
          bandwidth_scaler_ = std::unique_ptr<BandwidthScaler>(
              new BandwidthScaler(handler_.get()));
        },
        RTC_FROM_HERE);
  }

  ~BandwidthScalerTest() {
    task_queue_.SendTask([this] { bandwidth_scaler_ = nullptr; },
                         RTC_FROM_HERE);
  }

  void TriggerBandwidthScalerTest(ScaleDirection scale_direction,
                                  int width,
                                  int height) {
    absl::optional<VideoEncoder::ResolutionBitrateLimits> suitable_bitrate =
        bandwidth_scaler_->GetBitRateLimitedForResolution(width, height);

    if (!suitable_bitrate.has_value()) {
      RTC_LOG(LS_ERROR)
          << " TriggerBandwidthScalerTest can't find  suitable_bitrate";
      return;
    }

    float min_bitrate_bps = suitable_bitrate.value().min_bitrate_bps;
    float max_bitrate_bps = suitable_bitrate.value().max_bitrate_bps;

    for (int i = 0; i < kFramerateForTest * 5; ++i) {
      switch (scale_direction) {
        case kKeepScaleNormalBandwidth: {
          bandwidth_scaler_->ReportEncodeInfo(
              (min_bitrate_bps + max_bitrate_bps) / 8 / 2 / kFramerateForTest,
              0, width, height);
          break;
        }
        case kKeepScaleAboveMaxBandwidth: {
          bandwidth_scaler_->ReportEncodeInfo(
              max_bitrate_bps / 8 * 1.5 / kFramerateForTest, 0, width, height);
          break;
        }
        case kKeepScaleUnderMinBandwidth: {
          bandwidth_scaler_->ReportEncodeInfo(
              min_bitrate_bps / 8 / 2 / kFramerateForTest, 0, width, height);
          break;
        }
      }
    }

    return;
  }

  TaskQueueForTest task_queue_;
  std::unique_ptr<BandwidthScaler> bandwidth_scaler_;
  std::unique_ptr<FakeBandwidthHandler> handler_;
};

TEST_F(BandwidthScalerTest, KeepScaleNormalBandwidth_640X360) {
  task_queue_.SendTask(
      [this] {
        TriggerBandwidthScalerTest(kKeepScaleNormalBandwidth, 640, 360);
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(handler_->event_.Wait(6000));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(0, handler_->adapt_up_events_);
}

TEST_F(BandwidthScalerTest, KeepScaleAboveMaxBandwidth_640X360) {
  task_queue_.SendTask(
      [this] {
        TriggerBandwidthScalerTest(kKeepScaleAboveMaxBandwidth, 640, 360);
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(handler_->event_.Wait(5000 + 1000));
  task_queue_.SendTask(
      [this] {
        TriggerBandwidthScalerTest(kKeepScaleAboveMaxBandwidth, 640, 360);
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(handler_->event_.Wait(5000 + 1000));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(1, handler_->adapt_up_events_);
}

TEST_F(BandwidthScalerTest, KeepScaleUnderMinBandwidth_640X360) {
  task_queue_.SendTask(
      [this] {
        TriggerBandwidthScalerTest(kKeepScaleUnderMinBandwidth, 640, 360);
      },
      RTC_FROM_HERE);
  EXPECT_FALSE(handler_->event_.Wait(5000 + 1000));

  task_queue_.SendTask(
      [this] {
        TriggerBandwidthScalerTest(kKeepScaleUnderMinBandwidth, 640, 360);
      },
      RTC_FROM_HERE);
  EXPECT_TRUE(handler_->event_.Wait(5000 + 1000));
  EXPECT_EQ(1, handler_->adapt_down_events_);
  EXPECT_EQ(0, handler_->adapt_up_events_);
}
}  // namespace webrtc
