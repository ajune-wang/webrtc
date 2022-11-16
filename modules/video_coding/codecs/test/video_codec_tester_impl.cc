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

#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/codecs/test/video_codec_analyser.h"

namespace webrtc {
namespace test {

namespace {

using TestDecoder = VideoCodecTester::TestDecoder;
using TestEncoder = VideoCodecTester::TestEncoder;
using FrameSettings = VideoCodecTester::FrameSettings;

// A thread-safe video frame reader to be shared with the quality analyser that
// reads reference video frames from a separate thread.
class FrameReaderLocked : public VideoCodecTester::TestFrameReader {
 public:
  explicit FrameReaderLocked(std::unique_ptr<TestFrameReader> frame_reader)
      : frame_reader_(std::move(frame_reader)) {}

  absl::optional<VideoFrame> PullFrame() override {
    MutexLock lock(&mutex_);
    return frame_reader_->PullFrame();
  }

  absl::optional<VideoFrame> ReadFrame(size_t frame_num) override {
    MutexLock lock(&mutex_);
    return frame_reader_->ReadFrame(frame_num);
  }

  void Close() override {
    MutexLock lock(&mutex_);
    frame_reader_->Close();
  }

 protected:
  std::unique_ptr<TestFrameReader> frame_reader_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
};

class TesterTestDecoder {
 public:
  TesterTestDecoder(std::unique_ptr<TestDecoder> decoder,
                    VideoCodecAnalyser* analyser)
      : decoder_(std::move(decoder)), analyser_(analyser) {}

  void Decode(const EncodedImage& frame) {
    analyser_->StartDecode(frame);
    decoder_->Decode(frame, [this](const VideoFrame& decoded_frame) {
      this->analyser_->FinishDecode(decoded_frame, /*spatial_idx=*/0);
    });
  }

 protected:
  std::unique_ptr<TestDecoder> decoder_;
  VideoCodecAnalyser* const analyser_;
};

class TesterTestEncoder {
 public:
  TesterTestEncoder(std::unique_ptr<TestEncoder> encoder,
                    TesterTestDecoder* decoder,
                    VideoCodecAnalyser* analyser)
      : encoder_(std::move(encoder)), decoder_(decoder), analyser_(analyser) {}

  void Encode(const VideoFrame& frame) {
    analyser_->StartEncode(frame);
    encoder_->Encode(frame, [this](const EncodedImage& encoded_frame,
                                   const FrameSettings& frame_settings) {
      VideoCodecAnalyser::CodingSettings coding_settings;
      coding_settings.bitrate_kbps = frame_settings.bitrate_kbps;
      coding_settings.framerate_fps = frame_settings.framerate_fps;
      this->analyser_->FinishEncode(encoded_frame, coding_settings);
      this->decoder_->Decode(encoded_frame);
    });
  }

 protected:
  std::unique_ptr<TestEncoder> encoder_;
  TesterTestDecoder* const decoder_;
  VideoCodecAnalyser* const analyser_;
};

}  // namespace

std::unique_ptr<VideoCodecTestStats> VideoCodecTesterImpl::RunEncodeDecodeTest(
    std::unique_ptr<TestFrameReader> frame_reader,
    const TestSettings& test_settings,
    std::unique_ptr<TestEncoder> encoder,
    std::unique_ptr<TestDecoder> decoder) {
  FrameReaderLocked tester_frame_reader(std::move(frame_reader));
  VideoCodecAnalyser perf_analyser(&tester_frame_reader);
  TesterTestDecoder tester_decoder(std::move(decoder), &perf_analyser);
  TesterTestEncoder tester_encoder(std::move(encoder), &tester_decoder,
                                   &perf_analyser);

  while (auto frame = tester_frame_reader.PullFrame()) {
    tester_encoder.Encode(*frame);
  }

  return perf_analyser.GetStats();
}

}  // namespace test
}  // namespace webrtc
