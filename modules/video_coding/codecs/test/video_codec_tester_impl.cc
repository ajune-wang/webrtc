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

class FrameReaderWrapper : public VideoCodecAnalyser::VideoFrameProvider {
 public:
  explicit FrameReaderWrapper(std::unique_ptr<VideoFrameReader> frame_reader)
      : frame_reader_(std::move(frame_reader)), frame_num_(0) {}

  std::unique_ptr<VideoFrame> ReadFrame(uint32_t timestamp_rtp) {
    timestamp_rtp_to_frame_num_[timestamp_rtp] = frame_num_;
    return ReadFrame(frame_num_++, timestamp_rtp);
  }

 protected:
  std::unique_ptr<VideoFrame> GetFrame(uint32_t timestamp_rtp) override {
    int frame_num = timestamp_rtp_to_frame_num_[timestamp_rtp];
    return ReadFrame(frame_num, timestamp_rtp);
  }

  std::unique_ptr<VideoFrame> ReadFrame(int frame_num, uint32_t timestamp_rtp) {
    std::unique_ptr<VideoFrame> frame = frame_reader_->ReadFrame(frame_num);
    frame->set_timestamp(timestamp_rtp);
    return frame;
  }

  std::unique_ptr<VideoFrameReader> frame_reader_;
  std::map<uint32_t, int> timestamp_rtp_to_frame_num_;
  int frame_num_;
};

class TesterTestDecoder {
 public:
  TesterTestDecoder(std::unique_ptr<TestDecoder> decoder,
                    VideoCodecAnalyser* analyser)
      : decoder_(std::move(decoder)), analyser_(analyser) {}

  void Decode(const EncodedImage& frame) {
    decoder_->Decode(frame, [this](const VideoFrame& decoded_frame) {
      this->analyser_->DecodeFinished(decoded_frame, /*spatial_idx=*/0);
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
    encoder_->Encode(frame, [this](const EncodedImage& encoded_frame,
                                   const FrameSettings& frame_settings) {
      VideoCodecAnalyser::CodingSettings coding_settings;
      coding_settings.bitrate_kbps = frame_settings.bitrate_kbps;
      coding_settings.framerate_fps = frame_settings.framerate_fps;
      this->analyser_->EncodeFinished(encoded_frame, coding_settings);
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
    std::unique_ptr<VideoFrameReader> frame_reader,
    const TestSettings& test_settings,
    std::unique_ptr<TestEncoder> encoder,
    std::unique_ptr<TestDecoder> decoder) {
  FrameReaderWrapper local_frame_reader(std::move(frame_reader));
  VideoCodecAnalyser perf_analyser(&local_frame_reader);
  TesterTestDecoder test_decoder(std::move(decoder), &perf_analyser);
  TesterTestEncoder test_encoder(std::move(encoder), &test_decoder,
                                 &perf_analyser);

  uint32_t timestamp_rtp = 3000;

  for (int frame_num = 0; frame_num < test_settings.num_frames; ++frame_num) {
    auto frame = local_frame_reader.ReadFrame(timestamp_rtp);
    test_encoder.Encode(*frame);
    timestamp_rtp += 90000 / 30;
  }

  return perf_analyser.GetStats();
}

}  // namespace test
}  // namespace webrtc
