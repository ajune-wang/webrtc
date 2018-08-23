/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_AUDIO_STREAM_H_
#define TEST_SCENARIO_AUDIO_STREAM_H_
#include <memory>
#include <string>
#include <vector>

#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {

class SendAudioStream : public NetworkReceiverInterface {
 public:
  SendAudioStream(CallClient* sender,
                  AudioStreamConfig config,
                  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
                  Transport* send_transport);
  ~SendAudioStream();
  void Start();
  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver) override;

 private:
  friend class Scenario;
  friend class ReceiveAudioStream;
  AudioSendStream* send_stream_ = nullptr;
  CallClient* const sender_;
  const AudioStreamConfig config_;
  uint32_t ssrc_;
};

class ReceiveAudioStream : public NetworkReceiverInterface {
 public:
  ReceiveAudioStream(CallClient* receiver,
                     AudioStreamConfig config,
                     SendAudioStream* send_stream,
                     rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                     Transport* feedback_transport);
  ~ReceiveAudioStream();
  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver) override;

 private:
  friend class Scenario;
  AudioReceiveStream* receive_stream_ = nullptr;
  CallClient* const receiver_;
  const AudioStreamConfig config_;
};

class AudioStreamPair {
 public:
  SendAudioStream* send() { return &send_stream_; }
  ReceiveAudioStream* receive() { return &receive_stream_; }

 protected:
  friend class Scenario;
  AudioStreamPair(CallClient* sender,
                  std::vector<NetworkNode*> send_link,
                  uint64_t send_receiver_id,
                  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,

                  CallClient* receiver,
                  std::vector<NetworkNode*> return_link,
                  uint64_t return_receiver_id,
                  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                  AudioStreamConfig config)
      : config_(config),
        send_link_(send_link),
        return_link_(return_link),
        send_transport_(sender,
                        send_link.front(),
                        send_receiver_id,
                        config.stream.packet_overhead),
        return_transport_(receiver,
                          return_link.front(),
                          return_receiver_id,
                          config.stream.packet_overhead),
        send_stream_(sender, config, encoder_factory, &send_transport_),
        receive_stream_(receiver,
                        config,
                        &send_stream_,
                        decoder_factory,
                        &return_transport_) {
    NetworkNode::Route(send_transport_.ReceiverId(), &receive_stream_,
                       send_link_);
    NetworkNode::Route(return_transport_.ReceiverId(), &send_stream_,
                       return_link_);
  }

 private:
  const AudioStreamConfig config_;
  std::vector<NetworkNode*> send_link_;
  std::vector<NetworkNode*> return_link_;
  NetworkNodeTransport send_transport_;
  NetworkNodeTransport return_transport_;

  SendAudioStream send_stream_;
  ReceiveAudioStream receive_stream_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_AUDIO_STREAM_H_
