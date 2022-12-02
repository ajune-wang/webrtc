/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_tester_impl.h"

#include <map>
#include <memory>
#include <utility>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/video_codec_analyzer.h"
#include "rtc_base/event.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {
namespace test {

namespace {
using TestCodedVideoSource = VideoCodecTester::TestCodedVideoSource;
using TestRawVideoSource = VideoCodecTester::TestRawVideoSource;
using TestDecoder = VideoCodecTester::TestDecoder;
using TestEncoder = VideoCodecTester::TestEncoder;
using EncodeSettings = VideoCodecTester::EncodeSettings;
using DecodeSettings = VideoCodecTester::DecodeSettings;
using PacingSettings = VideoCodecTester::PacingSettings;
using PacingMode = VideoCodecTester::PacingMode;

// Reading frames from video source is happening in the main thread and
// encoding/decoding are happning in their own threads. This lets reading to go
// far ahead of encoding/decoding and buffer many raw video fames into memory.
// To prevent this we limit maximum number of encode/decode tasks.
constexpr int kMaxTaskQueueSize = 20;

constexpr Frequency k90kHz = Frequency::Hertz(90000);

// A thread-safe video frame reader to be shared with the quality analyzer
// which reads reference video frames from a separate thread.
class LockedRawVideoSource : public VideoCodecAnalyzer::ReferenceVideoSource {
 public:
  explicit LockedRawVideoSource(
      std::unique_ptr<TestRawVideoSource> video_source)
      : video_source_(std::move(video_source)) {}

  absl::optional<VideoFrame> PullFrame() {
    MutexLock lock(&mutex_);
    return video_source_->PullFrame();
  }

  VideoFrame GetFrame(uint32_t timestamp_rtp) override {
    MutexLock lock(&mutex_);
    return video_source_->GetFrame(timestamp_rtp);
  }

 protected:
  std::unique_ptr<TestRawVideoSource> video_source_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

// Pacing is implemented by posting encode/decode tasks into queue with delay.
// Delay in task queue is interpreted as absolute time from now (not time
// relative to previously posted task).
class Pacer {
 public:
  explicit Pacer(PacingSettings settings)
      : settings_(settings), delay_(TimeDelta::Zero()) {}
  TimeDelta Delay(Timestamp beat) {
    if (settings_.mode == PacingMode::kNoPacing) {
      return TimeDelta::Zero();
    }

    Timestamp now = Timestamp::Micros(rtc::TimeMicros());
    if (prev_time_.has_value()) {
      delay_ += PacingTime(beat);
      delay_ -= (now - *prev_time_);
      printf("\nft=%lld, pt=%lld, tp=%lld, delay_=%lld", beat.ms(),
             PacingTime(beat).ms(), (now - *prev_time_).ms(), delay_.ms());
      if (delay_.ns() < 0) {
        delay_ = TimeDelta::Zero();
      }
    }

    prev_beat_ = beat;
    prev_time_ = now;
    return delay_;
  }

 private:
  TimeDelta PacingTime(Timestamp beat) {
    if (settings_.mode == PacingMode::kRealTime) {
      return beat - *prev_beat_;
    }
    RTC_CHECK_EQ(PacingMode::kConstRate, settings_.mode);
    return 1 / settings_.rate;
  }

  PacingSettings settings_;
  absl::optional<Timestamp> prev_beat_;
  absl::optional<Timestamp> prev_time_;
  TimeDelta delay_;
};

class LimitedTaskQueue {
 public:
  LimitedTaskQueue() : queue_size_(0), max_queue_size_(kMaxTaskQueueSize) {}

  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       webrtc::TimeDelta delay) {
    ++queue_size_;
    task_queue_.PostDelayedTask(
        [this, task = std::move(task)]() mutable {
          --queue_size_;
          task_started_.Set();
          std::move(task)();
        },
        delay);
  }

  void WaitForPreviouslyPostedTasks() {
    while (queue_size_ > 0) {
      task_started_.Wait(rtc::Event::kForever);
    }
    task_queue_.WaitForPreviouslyPostedTasks();
  }

  TaskQueueForTest task_queue_;
  std::atomic_int queue_size_;
  const int max_queue_size_;
  rtc::Event task_started_;
};

class TesterTestDecoder {
 public:
  TesterTestDecoder(std::unique_ptr<TestDecoder> decoder,
                    VideoCodecAnalyzer* analyzer,
                    const DecodeSettings& settings)
      : decoder_(std::move(decoder)),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing) {}

  void Decode(const EncodedImage& frame) {
    Timestamp timestamp = Timestamp::Micros((frame.Timestamp() / k90kHz).us());
    task_queue_.PostDelayedTask(
        [this, frame = frame] {
          analyzer_->StartDecode(frame);
          decoder_->Decode(frame, [this](const VideoFrame& decoded_frame) {
            this->analyzer_->FinishDecode(decoded_frame, /*spatial_idx=*/0);
          });
        },
        pacer_.Delay(timestamp));

    if (settings_.pacing.mode == PacingMode::kNoPacing) {
      task_queue_.WaitForPreviouslyPostedTasks();
    }
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<TestDecoder> decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const DecodeSettings& settings_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
};

class TesterTestEncoder {
 public:
  TesterTestEncoder(std::unique_ptr<TestEncoder> encoder,
                    TesterTestDecoder* decoder,
                    VideoCodecAnalyzer* analyzer,
                    const EncodeSettings& settings)
      : encoder_(std::move(encoder)),
        decoder_(decoder),
        analyzer_(analyzer),
        settings_(settings),
        pacer_(settings.pacing) {}

  void Encode(const VideoFrame& frame) {
    Timestamp timestamp = Timestamp::Micros((frame.timestamp() / k90kHz).us());

    task_queue_.PostDelayedTask(
        [this, frame = frame] {
          analyzer_->StartEncode(frame);
          encoder_->Encode(frame, [this](const EncodedImage& encoded_frame) {
            this->analyzer_->FinishEncode(encoded_frame);
            this->decoder_->Decode(encoded_frame);
          });
        },
        pacer_.Delay(timestamp));

    if (settings_.pacing.mode == PacingMode::kNoPacing) {
      task_queue_.WaitForPreviouslyPostedTasks();
    }
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<TestEncoder> encoder_;
  TesterTestDecoder* const decoder_;
  VideoCodecAnalyzer* const analyzer_;
  const EncodeSettings& settings_;
  Pacer pacer_;
  LimitedTaskQueue task_queue_;
};

}  // namespace

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunDecodeTest(
    std::unique_ptr<TestCodedVideoSource> video_source,
    std::unique_ptr<TestDecoder> decoder,
    const DecodeSettings& decode_settings) {
  VideoCodecAnalyzer perf_analyzer(/*referece_video_source=*/nullptr);
  TesterTestDecoder tester_decoder(std::move(decoder), &perf_analyzer,
                                   decode_settings);

  while (auto frame = video_source->PullFrame()) {
    tester_decoder.Decode(*frame);
  }

  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunEncodeTest(
    std::unique_ptr<TestRawVideoSource> video_source,
    std::unique_ptr<TestEncoder> encoder,
    const EncodeSettings& encode_settings) {
  LockedRawVideoSource locked_source(std::move(video_source));
  VideoCodecAnalyzer perf_analyzer(&locked_source);
  TesterTestEncoder tester_encoder(std::move(encoder), /*decoder=*/nullptr,
                                   &perf_analyzer, encode_settings);

  while (auto frame = locked_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();

  return perf_analyzer.GetStats();
}

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    std::unique_ptr<TestRawVideoSource> video_source,
    std::unique_ptr<TestEncoder> encoder,
    std::unique_ptr<TestDecoder> decoder,
    const EncodeSettings& encode_settings,
    const DecodeSettings& decode_settings) {
  LockedRawVideoSource locked_source(std::move(video_source));
  VideoCodecAnalyzer perf_analyzer(&locked_source);
  TesterTestDecoder tester_decoder(std::move(decoder), &perf_analyzer,
                                   decode_settings);
  TesterTestEncoder tester_encoder(std::move(encoder), &tester_decoder,
                                   &perf_analyzer, encode_settings);

  while (auto frame = locked_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();
  tester_decoder.Flush();

  return perf_analyzer.GetStats();
}

}  // namespace test
}  // namespace webrtc
