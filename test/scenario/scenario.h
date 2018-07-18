/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_H_
#define TEST_SCENARIO_SCENARIO_H_
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "test/scenario/audio_stream.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"
#include "test/scenario/video_stream.h"

namespace webrtc {
namespace test {
class RepeatedActivity {
 public:
  void Stop();

 protected:
};

class TimeTrigger {
 public:
  explicit TimeTrigger(TimeDelta relative_time);

 protected:
  TimeDelta relative_time_;
};
// Scenario manages lifetimes etc.
class Scenario {
 public:
  Scenario();
  explicit Scenario(std::string log_path);

  LambdaPrinter TimePrinter();

  CallClient* CreateClient(std::string name, CallClientConfig config);
  NetworkNode* CreateNetworkNode();

  std::pair<SendVideoStream*, ReceiveVideoStream*> CreateVideoStreams(
      CallClient* sender,
      NetworkNode* send_link,
      CallClient* receiver,
      NetworkNode* return_link,
      VideoStreamConfig config);

  std::pair<SendAudioStream*, ReceiveAudioStream*> CreateAudioStreams(
      CallClient* sender,
      NetworkNode* send_link,
      CallClient* receiver,
      NetworkNode* return_link,
      AudioStreamConfig config);
  RepeatedActivity* Every(TimeDelta period,
                          std::function<void(TimeDelta)> function);
  RepeatedActivity* Every(TimeDelta period, std::function<void()> function);

  void RunFor(TimeDelta duration);
  void RunUntil(TimeDelta poll_interval, std::function<bool()> exit_function);
  Timestamp Now();

 private:
  std::string base_filename_;
  std::vector<std::unique_ptr<NetworkNodeTransport>> transports_;
  std::vector<std::unique_ptr<CallClient>> clients_;
  std::vector<std::unique_ptr<NetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<SendVideoStream>> send_video_streams_;
  std::vector<std::unique_ptr<ReceiveVideoStream>> receive_video_streams_;
  std::vector<std::unique_ptr<SendAudioStream>> send_audio_streams_;
  std::vector<std::unique_ptr<ReceiveAudioStream>> receive_audio_streams_;
  int64_t next_receiver_id_ = 40000;

  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_H_
