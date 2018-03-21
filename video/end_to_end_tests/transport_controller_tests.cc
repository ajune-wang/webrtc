/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"
/*
#include "modules/rtp_rtcp/source/byte_io.h"
#include "test/rtcp_packet_parser.h"
#include "video/end_to_end_tests/multi_stream_tester.h"
*/

namespace webrtc {
namespace {
static constexpr int kDisableVideoUnderBps = 30000;
static constexpr int kTimeoutMs = 1000;

struct RtpPacketTestInfo {
  RTPHeader header;
};

class TransportControllerStateMachineTest;
class TestSequenceState {
 public:
  explicit TestSequenceState(TransportControllerStateMachineTest* test_runner)
      : test_runner_(test_runner) {}
  virtual bool EnterState() { return false; }
  virtual ~TestSequenceState() = default;
  virtual bool OnSendRtp(const RtpPacketTestInfo& packet) { return true; }
  virtual std::string GetName() = 0;
  TransportControllerStateMachineTest* test_runner_;
};

class TransportControllerStateMachineTest : public test::EndToEndTest {
 public:
  using EndToEndTest::EndToEndTest;
  void AddState(std::unique_ptr<TestSequenceState>&& state) {
    RTC_CHECK_EQ(current_state_index_, 0) << "Can't add state at runtime";
    test_states_.push_back(std::move(state));
    current_state_ = test_states_[0].get();
  }
  void NextState() {
    rtc::CritScope cs(&state_crit_);
    current_state_index_++;
    if (current_state_index_ < test_states_.size()) {
      RTC_LOG(LS_INFO) << "State:" << current_state_index_ << "\n";
      current_state_ = test_states_[current_state_index_].get();
      // Note that this is recursive and therefore assumes that the max number
      // of states in sequence is small.
      if (current_state_->EnterState())
        NextState();
    } else {
      observation_complete_.Set();
    }
  }
  Action OnSendRtp(const uint8_t* packet, size_t length) final {
    RtpPacketTestInfo packet_info;
    EXPECT_TRUE(parser_->Parse(packet, length, &packet_info.header));
    return current_state_->OnSendRtp(packet_info) ? SEND_PACKET : DROP_PACKET;
  }
  void OnRtpTransportControllerSendCreated(
      RtpTransportControllerSend* controller) final {
    transport_controller_ = controller;
  }
  RtpTransportControllerSend* GetController() { return transport_controller_; }
  void PerformTest() final {
    EXPECT_TRUE(Wait()) << "Timed out while waiting for state: "
                        << current_state_->GetName();
  }

 private:
  rtc::CriticalSection state_crit_;
  std::vector<std::unique_ptr<TestSequenceState>> test_states_;
  size_t current_state_index_ = 0;
  TestSequenceState* current_state_;
  RtpTransportControllerSend* transport_controller_;
};

struct ExpectTransportSequenceNumber : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool OnSendRtp(const RtpPacketTestInfo& packet) override {
    EXPECT_TRUE(packet.header.extension.hasTransportSequenceNumber);
    test_runner_->NextState();
    return true;
  }
  std::string GetName() override { return "ExpectTransportSequenceNumber"; }
};
struct WaitForFirstVideoPacket : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool OnSendRtp(const RtpPacketTestInfo& packet) override {
    if (packet.header.ssrc == test::CallTest::kVideoSendSsrcs[0]) {
      EXPECT_TRUE(packet.header.extension.hasTransportSequenceNumber);
      test_runner_->NextState();
    }
    return true;
  }
  std::string GetName() override { return "WaitForFirstVideoPacket"; }
};
struct EnsureControllerHasPacketFeedback : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool OnSendRtp(const RtpPacketTestInfo& packet) override {
    bool has_packet_feedback =
        test_runner_->GetController()->GetHasPacketFeedbackForTest();
    if (has_packet_feedback)
      test_runner_->NextState();
    return true;
  }
  std::string GetName() override { return "EnsureHasPacketFeedback"; }
};
struct DisableVideoStream : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool EnterState() override {
    BitrateConstraintsMask constraints;
    constraints.max_bitrate_bps = kDisableVideoUnderBps / 2;
    test_runner_->GetController()->SetClientBitratePreferences(constraints);
    return true;
  }
  std::string GetName() override { return "DisableVideoStream"; }
};
struct WaitForNoPacketFeedback : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool OnSendRtp(const RtpPacketTestInfo& packet) override {
    bool has_packet_feedback =
        test_runner_->GetController()->GetHasPacketFeedbackForTest();
    if (!has_packet_feedback)
      test_runner_->NextState();
    return true;
  }
  std::string GetName() override { return "WaitForNoPacketFeedback"; }
};
struct EnableVideoStream : TestSequenceState {
  using TestSequenceState::TestSequenceState;
  bool EnterState() override {
    BitrateConstraintsMask constraints;
    constraints.max_bitrate_bps = kDisableVideoUnderBps * 2;
    test_runner_->GetController()->SetClientBitratePreferences(constraints);
    return true;
  }
  std::string GetName() override { return "EnableVideoStream"; }
};
}  // namespace
class TransportControllerEndToEndTest : public test::CallTest {
 public:
  TransportControllerEndToEndTest() {}

  virtual ~TransportControllerEndToEndTest() {
    EXPECT_EQ(nullptr, video_send_stream_);
  }

 protected:
};

TEST_F(TransportControllerEndToEndTest, UpdatesTransportFeedbackAvailability) {
  class ToggleVideoTest : public TransportControllerStateMachineTest {
   public:
    ToggleVideoTest() : TransportControllerStateMachineTest(kTimeoutMs) {
      AddState(rtc::MakeUnique<WaitForFirstVideoPacket>(this));
      AddState(rtc::MakeUnique<EnsureControllerHasPacketFeedback>(this));
      AddState(rtc::MakeUnique<DisableVideoStream>(this));
      AddState(rtc::MakeUnique<WaitForNoPacketFeedback>(this));
      AddState(rtc::MakeUnique<EnableVideoStream>(this));
      AddState(rtc::MakeUnique<EnsureControllerHasPacketFeedback>(this));
    }

    size_t GetNumVideoStreams() const override { return 1; }
    size_t GetNumAudioStreams() const override { return 1; }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      // Ensure that no feedback is used for Audio.
      send_config->rtp.extensions.clear();
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      send_config->suspend_below_min_bitrate = true;
    }
  } test;
  RunBaseTest(&test);
}

TEST_F(TransportControllerEndToEndTest, DetectsTransportFeedbackForAudio) {
  static constexpr int kExtensionId = 8;
  class ExpectPacketFeedbackTest : public TransportControllerStateMachineTest {
   public:
    ExpectPacketFeedbackTest()
        : TransportControllerStateMachineTest(kTimeoutMs) {
      AddState(rtc::MakeUnique<ExpectTransportSequenceNumber>(this));
      AddState(rtc::MakeUnique<EnsureControllerHasPacketFeedback>(this));
      parser_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                          kExtensionId);
    }

    size_t GetNumVideoStreams() const override { return 0; }
    size_t GetNumAudioStreams() const override { return 1; }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      // Ensure that feedback is used for Audio
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));
      // Audio send stream won't register with transport controller unless a
      // is has bitrate constraints.
      send_config->min_bitrate_bps = 0;
      send_config->max_bitrate_bps = 100000;
    }
  } test;
  RunBaseTest(&test);
}

}  // namespace webrtc
