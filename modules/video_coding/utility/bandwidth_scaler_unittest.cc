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
#include "rtc_base/time_utils.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
static const int kFramerateForTest = 30;
static const int kDefaultBitrateStateUpdateIntervalSeconds = 5;
static const int kDefaultEncodeDeltaTimeMs = 33;  // 1/30(s) => 33(ms)

}  // namespace

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

class BandwidthScalerUnderTest : public BandwidthScaler {
 public:
  explicit BandwidthScalerUnderTest(
      BandwidthScalerUsageHandlerInterface* handler)
      : BandwidthScaler(handler) {}

  int GetBitrateStateUpdateIntervalMs() {
    return this->kBitrateStateUpdateInterval.ms() + 200;
  }
};

class BandwidthScalerTest : public ::testing::Test,
                            public ::testing::WithParamInterface<std::string> {
 protected:
  enum ScaleDirection {
    kKeepScaleNormalBandwidth,
    kKeepScaleAboveMaxBandwidth,
    kKeepScaleUnderMinBandwidth,
  };

  enum FrameType {
    kKeyFrame,
    kNormalFrame,
    kNormalFrame_Overuse,
    kNormalFrame_Underuse,
  };
  struct CreateFrameConfig {
    CreateFrameConfig(int frame_num,
                      FrameType frame_type,
                      int actual_width,
                      int actual_height)
        : frame_num(frame_num),
          frame_type(frame_type),
          actual_width(actual_width),
          actual_height(actual_height) {}

    int frame_num;
    FrameType frame_type;
    int actual_width;
    int actual_height;
  };

  BandwidthScalerTest()
      : scoped_field_trial_(GetParam()),
        task_queue_("BandwidthScalerTestQueue"),
        handler_(std::make_unique<FakeBandwidthHandler>()),
        time_send_to_scaler_ms_(0) {
    task_queue_.SendTask(
        [this] {
          bandwidth_scaler_ = std::unique_ptr<BandwidthScalerUnderTest>(
              new BandwidthScalerUnderTest(handler_.get()));
          // Only for testing. Set first_timestamp_ in RateStatistics to 0.
          bandwidth_scaler_->ReportEncodeInfo(0, 0, 0, 0);
        },
        RTC_FROM_HERE);
  }

  ~BandwidthScalerTest() {
    task_queue_.SendTask([this] { bandwidth_scaler_ = nullptr; },
                         RTC_FROM_HERE);
  }

  int GetFrameSize(
      const CreateFrameConfig& config,
      const VideoEncoder::ResolutionBitrateLimits& bitrate_limits) {
    int frame_size = 0;
    int scale = 8 * kFramerateForTest;
    switch (config.frame_type) {
      case FrameType::kKeyFrame: {
        // 4 is experimental value. Based on the test, the number of bytes of
        // the key frame is about four times of the normal frame
        frame_size = bitrate_limits.max_bitrate_bps * 4 / scale;
        break;
      }
      case FrameType::kNormalFrame_Overuse: {
        frame_size = bitrate_limits.max_bitrate_bps * 3 / 2 / scale;
        break;
      }
      case FrameType::kNormalFrame_Underuse: {
        frame_size = bitrate_limits.min_bitrate_bps * 3 / 4 / scale;
        break;
      }
      case FrameType::kNormalFrame: {
        frame_size =
            (bitrate_limits.max_bitrate_bps + bitrate_limits.min_bitrate_bps) /
            2 / scale;
        break;
      }
    }
    return frame_size;
  }

  void TriggerBandwidthScalerTest(
      const std::vector<CreateFrameConfig>& CreateFrameConfigs) {
    RTC_CHECK(!CreateFrameConfigs.empty());

    int tot_frame_nums = 0;
    for (size_t i = 0; i < CreateFrameConfigs.size(); ++i) {
      tot_frame_nums += CreateFrameConfigs[i].frame_num;
    }

    EXPECT_EQ(kFramerateForTest * kDefaultBitrateStateUpdateIntervalSeconds,
              tot_frame_nums);

    absl::optional<VideoEncoder::ResolutionBitrateLimits> suitable_bitrate =
        bandwidth_scaler_->GetBitrateLimitsForResolution(
            CreateFrameConfigs[0].actual_width,
            CreateFrameConfigs[0].actual_height);

    EXPECT_TRUE(suitable_bitrate);

    time_send_to_scaler_ms_ = rtc::TimeMillis();
    for (size_t i = 0; i < CreateFrameConfigs.size(); ++i) {
      const CreateFrameConfig& config = CreateFrameConfigs[i];
      for (int j = 0; j <= config.frame_num; ++j) {
        time_send_to_scaler_ms_ += kDefaultEncodeDeltaTimeMs;
        int frame_size = GetFrameSize(config, suitable_bitrate.value());
        bandwidth_scaler_->ReportEncodeInfo(frame_size, time_send_to_scaler_ms_,
                                            config.actual_width,
                                            config.actual_height);
      }
    }
  }

  test::ScopedFieldTrials scoped_field_trial_;
  TaskQueueForTest task_queue_;
  std::unique_ptr<BandwidthScalerUnderTest> bandwidth_scaler_;
  std::unique_ptr<FakeBandwidthHandler> handler_;
  uint32_t time_send_to_scaler_ms_;
};

INSTANTIATE_TEST_SUITE_P(
    FieldTrials,
    BandwidthScalerTest,
    ::testing::Values("WebRTC-Video-BandwidthScalerSettings/"
                      "bitrate_state_update_interval:1/",
                      "WebRTC-Video-BandwidthScalerSettings/"
                      "bitrate_state_update_interval:2/"));

TEST_P(BandwidthScalerTest, AllNoramlFrame_640X360) {
  task_queue_.SendTask(
      [this] {
        const std::vector<CreateFrameConfig> CreateFrameConfigs{
            CreateFrameConfig(150, FrameType::kNormalFrame, 640, 360)};
        TriggerBandwidthScalerTest(CreateFrameConfigs);
      },
      RTC_FROM_HERE);

  // When resolution is 640*360,bps experimental value interval is
  // [500000,800000] BandwidthScaler calculates actual encode bps is 654253, so
  // it falls in the range without any operation(up/down)
  EXPECT_FALSE(handler_->event_.Wait(
      bandwidth_scaler_->GetBitrateStateUpdateIntervalMs()));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(0, handler_->adapt_up_events_);
}

TEST_P(BandwidthScalerTest, AllNoramlFrame_AboveMaxBandwidth_640X360) {
  task_queue_.SendTask(
      [this] {
        const std::vector<CreateFrameConfig> CreateFrameConfigs{
            CreateFrameConfig(150, FrameType::kNormalFrame_Overuse, 640, 360)};
        TriggerBandwidthScalerTest(CreateFrameConfigs);
      },
      RTC_FROM_HERE);

  // When resolution is 640*360,bps experimental value interval is
  // [500000,800000] BandwidthScaler calculates actual encode bps is 1208000, so
  // it triggers adapt_up_events_
  EXPECT_TRUE(handler_->event_.Wait(
      bandwidth_scaler_->GetBitrateStateUpdateIntervalMs()));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(1, handler_->adapt_up_events_);
}

TEST_P(BandwidthScalerTest, AllNormalFrame_Underuse_640X360) {
  task_queue_.SendTask(
      [this] {
        const std::vector<CreateFrameConfig> CreateFrameConfigs{
            CreateFrameConfig(150, FrameType::kNormalFrame_Underuse, 640, 360)};
        TriggerBandwidthScalerTest(CreateFrameConfigs);
      },
      RTC_FROM_HERE);

  // When resolution is 640*360,bps experimental value interval is
  // [500000,800000] BandwidthScaler calculates actual encode bps is 377379, so
  // it triggers adapt_down_events_
  EXPECT_TRUE(handler_->event_.Wait(
      bandwidth_scaler_->GetBitrateStateUpdateIntervalMs()));
  EXPECT_EQ(1, handler_->adapt_down_events_);
  EXPECT_EQ(0, handler_->adapt_up_events_);
}

TEST_P(BandwidthScalerTest, FixedFrameTypeTest1_640X360) {
  task_queue_.SendTask(
      [this] {
        const std::vector<CreateFrameConfig> CreateFrameConfigs{
            CreateFrameConfig(10, FrameType::kNormalFrame_Underuse, 640, 360),
            CreateFrameConfig(110, FrameType::kNormalFrame, 640, 360),
            CreateFrameConfig(25, FrameType::kNormalFrame_Overuse, 640, 360),
            CreateFrameConfig(5, FrameType::kKeyFrame, 640, 360),
        };
        TriggerBandwidthScalerTest(CreateFrameConfigs);
      },
      RTC_FROM_HERE);

  // When resolution is 640*360,bps experimental value interval is
  // [500000,800000] BandwidthScaler calculates actual encode bps is 839430, so
  // it triggers adapt_up_events_
  EXPECT_TRUE(handler_->event_.Wait(
      bandwidth_scaler_->GetBitrateStateUpdateIntervalMs()));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(1, handler_->adapt_up_events_);
}

TEST_P(BandwidthScalerTest, FixedFrameTypeTest2_640X360) {
  task_queue_.SendTask(
      [this] {
        const std::vector<CreateFrameConfig> CreateFrameConfigs{
            CreateFrameConfig(10, FrameType::kNormalFrame_Underuse, 640, 360),
            CreateFrameConfig(50, FrameType::kNormalFrame, 640, 360),
            CreateFrameConfig(5, FrameType::kKeyFrame, 640, 360),
            CreateFrameConfig(85, FrameType::kNormalFrame_Overuse, 640, 360),
        };
        TriggerBandwidthScalerTest(CreateFrameConfigs);
      },
      RTC_FROM_HERE);

  // When resolution is 640*360,bps experimental value interval is
  // [500000,800000] BandwidthScaler calculates actual encode bps is 1059462, so
  // it triggers adapt_up_events_
  EXPECT_TRUE(handler_->event_.Wait(
      bandwidth_scaler_->GetBitrateStateUpdateIntervalMs()));
  EXPECT_EQ(0, handler_->adapt_down_events_);
  EXPECT_EQ(1, handler_->adapt_up_events_);
}

}  // namespace webrtc
