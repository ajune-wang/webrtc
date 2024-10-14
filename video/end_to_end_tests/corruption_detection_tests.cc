/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/gtest.h"

namespace webrtc {

class CorruptionDetectionTest : public test::CallTest {
 public:
  CorruptionDetectionTest() {
    RegisterRtpExtension(
        RtpExtension(RtpExtension::kCorruptionDetectionUri, 1, true));
  }
};

TEST_F(CorruptionDetectionTest, Foo) {
  class StatsObserver : public test::EndToEndTest {
   public:
    StatsObserver()
        : EndToEndTest(test::VideoTestConstants::kLongTimeout),
          encoder_factory_(
              [](const Environment& env, const SdpVideoFormat& format) {
                return CreateVp8Encoder(env);
              }),
          decoder_factory_(
              [](const Environment& env, const SdpVideoFormat& format) {
                return CreateVp8Decoder(env);
              }) {}

   private:
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // TODO: Consider generalizing instead of hard-coding VP8.
      encoder_config->codec_type = kVideoCodecVP8;
      send_config->encoder_settings.enable_frame_instrumentation_generator =
          true;
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.payload_name = "VP8";
      send_config->rtp.extensions.clear();
      // TODO: Define the extension instead of repeating it.
      send_config->rtp.extensions.push_back(
          RtpExtension(RtpExtension::kCorruptionDetectionUri, 1, true));

      // send_config->rtp.payload_type =
      // test::VideoTestConstants::kVideoSendPayloadType;
      for (auto& receive_config : *receive_configs) {
        receive_config.decoder_factory = &decoder_factory_;
        receive_config.decoders[0].video_format =
            SdpVideoFormat(send_config->rtp.payload_name);
      }
    }

    void OnVideoStreamsCreated(VideoSendStream* send_stream,
                               const std::vector<VideoReceiveStreamInterface*>&
                                   receive_streams) override {
      receive_streams_ = receive_streams;
      task_queue_ = TaskQueueBase::Current();
    }

    void PerformTest() override {
      // TODO: Check for `corruption_score_count > 0` condition in a loop up to
      // some deadline amount of time, and sleep a short (10ms?) after each
      // iteration.
      SleepMs(1000);
      SendTask(task_queue_, [&]() {
        for (const auto& receive_stream : receive_streams_) {
          EXPECT_GT(receive_stream->GetStats().corruption_score_count, 0u);
          // TODO: Verify score somehow?
        }
      });
    }

    std::vector<VideoReceiveStreamInterface*> receive_streams_;
    TaskQueueBase* task_queue_ = nullptr;
    test::FunctionVideoEncoderFactory encoder_factory_;
    test::FunctionVideoDecoderFactory decoder_factory_;
  } test;

  RunBaseTest(&test);
}

}  // namespace webrtc
