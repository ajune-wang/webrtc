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

#include "test/frame_generator_capturer.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"
#include "test/test_video_capturer.h"

namespace webrtc {
namespace test {

class SendVideoStream : public NetworkReceiverInterface {
 public:
  SendVideoStream(CallClient* sender,
                  VideoStreamConfig config,
                  Transport* send_transport);
  ~SendVideoStream();
  void Start();

  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver) override;
  void SetCaptureFramerate(int framerate);
  void SetMaxFramerate(absl::optional<int> max_framerate);
  VideoSendStream::Stats GetStats() const;
  ColumnPrinter StatsPrinter();

 protected:
  friend class Scenario;
  friend class ReceiveVideoStream;
  std::vector<uint32_t> ssrcs_;
  std::vector<uint32_t> rtx_ssrcs_;
  VideoSendStream* send_stream_ = nullptr;
  CallClient* const sender_;
  const VideoStreamConfig config_;
  std::unique_ptr<VideoEncoderFactory> encoder_factory_;
  std::unique_ptr<TestVideoCapturer> video_capturer_;
  FrameGeneratorCapturer* frame_generator_ = nullptr;
};

class ReceiveVideoStream : public NetworkReceiverInterface {
 public:
  ReceiveVideoStream(CallClient* receiver,
                     VideoStreamConfig config,
                     SendVideoStream* send_stream,
                     size_t chosen_stream,
                     Transport* feedback_transport);
  ~ReceiveVideoStream();
  bool TryDeliverPacket(rtc::CopyOnWriteBuffer packet,
                        uint64_t receiver) override;

 protected:
  friend class Scenario;
  VideoReceiveStream* receive_stream_ = nullptr;
  FlexfecReceiveStream* flecfec_stream_ = nullptr;
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> renderer_;
  CallClient* const receiver_;
  const VideoStreamConfig config_;
  std::unique_ptr<VideoDecoder> decoder_;
};

class VideoStreamPair {
 public:
  SendVideoStream* send() { return &send_stream_; }
  ReceiveVideoStream* receive() { return &receive_stream_; }

 protected:
  friend class Scenario;
  VideoStreamPair(CallClient* sender,
                  std::vector<NetworkNode*> send_link,
                  uint64_t send_receiver_id,
                  CallClient* receiver,
                  std::vector<NetworkNode*> return_link,
                  uint64_t return_receiver_id,
                  VideoStreamConfig config)
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
        send_stream_(sender, config, &send_transport_),
        receive_stream_(receiver,
                        config,
                        &send_stream_,
                        /*chosen_stream=*/0,
                        &return_transport_) {
    NetworkNode::Route(send_transport_.ReceiverId(), &receive_stream_,
                       send_link_);
    NetworkNode::Route(return_transport_.ReceiverId(), &send_stream_,
                       return_link_);
  }

 private:
  const VideoStreamConfig config_;
  std::vector<NetworkNode*> send_link_;
  std::vector<NetworkNode*> return_link_;
  NetworkNodeTransport send_transport_;
  NetworkNodeTransport return_transport_;

  SendVideoStream send_stream_;
  ReceiveVideoStream receive_stream_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_VIDEO_STREAM_H_
