/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_END_TO_END_TESTS_CONGESTION_CONTROLLER_TEST_H_
#define VIDEO_END_TO_END_TESTS_CONGESTION_CONTROLLER_TEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/congestion_controller/test/controller_printer.h"
#include "test/call_test.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

class CallStatsPrinter {
 public:
  explicit CallStatsPrinter(std::string filename);
  CallStatsPrinter();
  ~CallStatsPrinter();
  void PrintHeaders();

  void PrintStats(int64_t time_ms,
                  int64_t pacer_delay_ms,
                  int64_t target_bitrate_bps,
                  int64_t media_bitrate_bps);

 private:
  FILE* output_file_ = nullptr;
  FILE* output_ = nullptr;
};

class BaseCongestionControllerTest : public EndToEndTest {
 public:
  BaseCongestionControllerTest(unsigned int timeout_ms,
                               std::string filepath_base);
  ~BaseCongestionControllerTest();
  void OnCallsCreated(Call* sender_call, Call* receiver_call) override;

  void OnVideoStreamsCreated(
      VideoSendStream* send_stream,
      const std::vector<VideoReceiveStream*>& receive_streams) override;

  virtual std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
                    std::unique_ptr<DebugStatePrinter>>
  CreateSendCCFactory(RtcEventLog* event_log);
  virtual std::pair<std::unique_ptr<NetworkControllerFactoryInterface>,
                    std::unique_ptr<DebugStatePrinter>>
  CreateReturnCCFactory(RtcEventLog* event_log);
  Call::Config GetSenderCallConfig() override;
  Call::Config GetReceiverCallConfig() override;
  void PrintStates(int64_t timestamp_ms);
  void PrintStats(int64_t timestamp_ms);

 protected:
  Call* sender_call_ = nullptr;
  Call* return_call_ = nullptr;
  const std::string filepath_base_;

 private:
  FILE* send_state_out_ = nullptr;
  FILE* return_state_out_ = nullptr;
  std::unique_ptr<CallStatsPrinter> send_stats_printer_;
  std::unique_ptr<CallStatsPrinter> return_stats_printer_;
  std::unique_ptr<ControlStatePrinter> send_printer_;
  std::unique_ptr<ControlStatePrinter> return_printer_;
  std::unique_ptr<NetworkControllerFactoryInterface> send_cc_factory_;
  std::unique_ptr<NetworkControllerFactoryInterface> return_cc_factory_;
  std::unique_ptr<RtcEventLog> send_event_log_;
  std::unique_ptr<RtcEventLog> recv_event_log_;

  VideoSendStream* send_stream_ = nullptr;
  std::vector<VideoReceiveStream*> receive_streams_;
};
}  // namespace test
}  // namespace webrtc

#endif  // VIDEO_END_TO_END_TESTS_CONGESTION_CONTROLLER_TEST_H_
