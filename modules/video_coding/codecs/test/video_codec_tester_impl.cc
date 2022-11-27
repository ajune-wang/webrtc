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
#include "modules/video_coding/codecs/test/video_codec_analyser.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/testsupport/yuv_frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using TestCodedVideoSource = VideoCodecTester::TestCodedVideoSource;
using TestRawVideoSource = VideoCodecTester::TestRawVideoSource;
using TestDecoder = VideoCodecTester::TestDecoder;
using TestEncoder = VideoCodecTester::TestEncoder;
using TestSettings = VideoCodecTester::TestSettings;
using FrameSettings = VideoCodecTester::FrameSettings;

const Frequency k90Hz = Frequency::Hertz(90000);

// A thread-safe video frame reader to be shared with the quality analyser
// which reads reference video frames from a separate thread.
class LockedRawVideoSource : public VideoCodecAnalyser::ReferenceVideoSource {
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

// Pacer guarantees that the minumum walltime delta between two consequetive
// Pace calls is equal to the pacing cycle.
class Pacer {
 public:
  void Pace(Timestamp event) {
    if (prev_event_.has_value()) {
      // Fake clock firendly implimentation.
      int64_t left_ms;
      while ((left_ms = TimeUntilNextCycle(event)) > 0) {
        SleepMs(left_ms);
      }
    }
    prev_event_ = event;
    prev_time_ = Timestamp::Micros(rtc::TimeMicros());
  }

 private:
  int64_t TimeUntilNextCycle(Timestamp event) {
    TimeDelta cycle = event - *prev_event_;
    TimeDelta elapsed = Timestamp::Micros(rtc::TimeMicros()) - *prev_time_;
    return (cycle - elapsed).ms();
  }

  absl::optional<Timestamp> prev_event_;
  absl::optional<Timestamp> prev_time_;
};

class TesterTestDecoder {
 public:
  TesterTestDecoder(std::unique_ptr<TestDecoder> decoder,
                    VideoCodecAnalyser* analyser,
                    const VideoCodecTesterImpl::TestSettings& test_settings)
      : decoder_(std::move(decoder)),
        analyser_(analyser),
        test_settings_(test_settings) {}

  void Decode(const EncodedImage& frame) {
    task_queue_.PostTask([this, frame = frame] {
      if (test_settings_.realtime_decoding) {
        Timestamp ts = Timestamp::Micros((frame.Timestamp() / k90Hz).us());
        pacer_.Pace(ts);
      }

      analyser_->StartDecode(frame);
      decoder_->Decode(frame, [this](const VideoFrame& decoded_frame) {
        this->analyser_->FinishDecode(decoded_frame, /*spatial_idx=*/0);
      });
    });
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<TestDecoder> decoder_;
  VideoCodecAnalyser* const analyser_;
  const VideoCodecTesterImpl::TestSettings& test_settings_;
  Pacer pacer_;
  TaskQueueForTest task_queue_;
};

class TesterTestEncoder {
 public:
  TesterTestEncoder(std::unique_ptr<TestEncoder> encoder,
                    TesterTestDecoder* decoder,
                    VideoCodecAnalyser* analyser,
                    const VideoCodecTesterImpl::TestSettings& test_settings)
      : encoder_(std::move(encoder)),
        decoder_(decoder),
        analyser_(analyser),
        test_settings_(test_settings) {}

  void Encode(const VideoFrame& frame) {
    task_queue_.PostTask([this, frame = frame] {
      if (test_settings_.realtime_encoding) {
        Timestamp ts = Timestamp::Micros((frame.timestamp() / k90Hz).us());
        pacer_.Pace(ts);
      }

      analyser_->StartEncode(frame);
      encoder_->Encode(frame, [this](const EncodedImage& encoded_frame,
                                     const FrameSettings& frame_settings) {
        VideoCodecAnalyser::CodingSettings coding_settings;
        coding_settings.bitrate_kbps = frame_settings.bitrate_kbps;
        coding_settings.framerate_fps = frame_settings.framerate_fps;
        this->analyser_->FinishEncode(encoded_frame, coding_settings);
        this->decoder_->Decode(encoded_frame);
      });
    });
  }

  void Flush() { task_queue_.WaitForPreviouslyPostedTasks(); }

 protected:
  std::unique_ptr<TestEncoder> encoder_;
  TesterTestDecoder* const decoder_;
  VideoCodecAnalyser* const analyser_;
  const VideoCodecTesterImpl::TestSettings& test_settings_;
  Pacer pacer_;
  TaskQueueForTest task_queue_;
};

}  // namespace

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunDecodeTest(
    std::unique_ptr<TestCodedVideoSource> video_source,
    const TestSettings& test_settings,
    std::unique_ptr<TestDecoder> decoder) {
  VideoCodecAnalyser perf_analyser(/*referece_video_source=*/nullptr);
  TesterTestDecoder tester_decoder(std::move(decoder), &perf_analyser,
                                   test_settings);

  while (auto frame = video_source->PullFrame()) {
    tester_decoder.Decode(*frame);
  }

  tester_decoder.Flush();

  return perf_analyser.GetStats();
}

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunEncodeTest(
    std::unique_ptr<TestRawVideoSource> video_source,
    const TestSettings& test_settings,
    std::unique_ptr<TestEncoder> encoder) {
  LockedRawVideoSource locked_source(std::move(video_source));
  VideoCodecAnalyser perf_analyser(&locked_source);
  TesterTestEncoder tester_encoder(std::move(encoder), /*decoder=*/nullptr,
                                   &perf_analyser, test_settings);

  while (auto frame = locked_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();

  return perf_analyser.GetStats();
}

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    std::unique_ptr<TestRawVideoSource> video_source,
    const TestSettings& test_settings,
    std::unique_ptr<TestEncoder> encoder,
    std::unique_ptr<TestDecoder> decoder) {
  LockedRawVideoSource locked_source(std::move(video_source));
  VideoCodecAnalyser perf_analyser(&locked_source);
  TesterTestDecoder tester_decoder(std::move(decoder), &perf_analyser,
                                   test_settings);
  TesterTestEncoder tester_encoder(std::move(encoder), &tester_decoder,
                                   &perf_analyser, test_settings);

  while (auto frame = locked_source.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  tester_encoder.Flush();
  tester_decoder.Flush();

  return perf_analyser.GetStats();
}

}  // namespace test
}  // namespace webrtc
