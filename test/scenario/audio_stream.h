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
  bool TrySendPacket(rtc::CopyOnWriteBuffer packet, uint64_t receiver) override;

 private:
  AudioSendStream* send_stream_ = nullptr;
  CallClient* const sender_;
};

class ReceiveAudioStream : public NetworkReceiverInterface {
 public:
  ReceiveAudioStream(CallClient* receiver,
                     AudioStreamConfig config,
                     rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                     Transport* feedback_transport);
  ~ReceiveAudioStream();
  bool TrySendPacket(rtc::CopyOnWriteBuffer packet, uint64_t receiver) override;

 private:
  AudioReceiveStream* receive_stream_ = nullptr;
  CallClient* const receiver_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_AUDIO_STREAM_H_
