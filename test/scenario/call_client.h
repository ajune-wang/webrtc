/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_CALL_CLIENT_H_
#define TEST_SCENARIO_CALL_CLIENT_H_
#include <memory>
#include <string>
#include <vector>

#include "test/scenario/column_printer.h"
#include "test/scenario/scenario_config.h"

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "modules/congestion_controller/test/controller_printer.h"

namespace webrtc {

namespace test {
// CallClient represents a participant in a call scenario. It is created by the
// Scenario class and is used as sender and receiver when setting up a media
// stream session.
class CallClient {
 public:
  CallClient(std::string name,
             CallClientConfig config,
             std::string base_filename);
  ~CallClient();
  ColumnPrinter StatsPrinter();
  Call::Stats GetStats();

 protected:
  friend class Scenario;
  friend class SendVideoStream;
  friend class ReceiveVideoStream;
  friend class SendAudioStream;
  friend class ReceiveAudioStream;
  friend class NetworkNodeTransport;
  uint32_t GetNextVideoSsrc();
  uint32_t GetNextAudioSsrc();
  uint32_t GetNextRtxSsrc();
  // TODO(srte): Consider using the Columnprinter interface for this.
  void LogCongestionControllerStats(Timestamp at_time);
  void DeliverPacket(MediaType media_type,
                     rtc::CopyOnWriteBuffer packet,
                     Timestamp at_time);
  std::unique_ptr<Call> call_;

 private:
  rtc::scoped_refptr<AudioState> InitAudio();

  rtc::scoped_refptr<AudioProcessing> apm_;
  rtc::scoped_refptr<TestAudioDeviceModule> fake_audio_device_;

  std::unique_ptr<FecControllerFactoryInterface> fec_controller_factory_;
  std::unique_ptr<RtcEventLog> event_log_;
  std::unique_ptr<NetworkControllerFactoryInterface> cc_factory_;
  std::unique_ptr<ControlStatePrinter> cc_printer_;
  FILE* cc_out_ = nullptr;
  int next_video_ssrc_index_ = 0;
  int next_rtx_ssrc_index_ = 0;
  int next_audio_ssrc_index_ = 0;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_CALL_CLIENT_H_
