/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_VIDEO_STREAM_H_
#define TEST_SCENARIO_VIDEO_STREAM_H_
#include <memory>
#include <string>
#include <vector>

#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"

namespace webrtc {
namespace test {

class SendVideoStream : public NetworkReceiverInterface {
 public:
  SendVideoStream(CallClient* sender,
                  VideoStreamConfig config,
                  Transport* send_transport);
  ~SendVideoStream();
  bool TrySendPacket(rtc::CopyOnWriteBuffer packet, uint64_t receiver) override;
  LambdaPrinter StatsPrinter();

 protected:
  VideoSendStream* send_stream_ = nullptr;
  VideoCapturer* video_source_ = nullptr;
  CallClient* const sender_;
  std::unique_ptr<VideoEncoderFactory> encoder_factory_;
};

class ReceiveVideoStream : public NetworkReceiverInterface {
 public:
  ReceiveVideoStream(CallClient* receiver,
                     VideoStreamConfig config,
                     size_t chosen_stream,
                     Transport* feedback_transport);
  ~ReceiveVideoStream();
  bool TrySendPacket(rtc::CopyOnWriteBuffer packet, uint64_t receiver) override;

 protected:
  VideoReceiveStream* receive_stream_ = nullptr;
  FlexfecReceiveStream* flecfec_stream_ = nullptr;
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> renderer_;
  CallClient* const receiver_;
  std::unique_ptr<VideoDecoder> decoder_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_VIDEO_STREAM_H_
