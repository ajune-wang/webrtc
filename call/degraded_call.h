/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_DEGRADED_CALL_H_
#define CALL_DEGRADED_CALL_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/call/transport.h"
#include "api/test/simulated_network.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "modules/utility/include/process_thread.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class DegradedCall : public Call,
                     private Transport,
                     private PacketReceiver,
                     private Module {
 public:
  explicit DegradedCall(
      std::unique_ptr<Call> call,
      std::vector<DefaultNetworkSimulationConfig> send_configs,
      std::vector<DefaultNetworkSimulationConfig> receive_configs);
  ~DegradedCall() override;

  // Implements Call.
  AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(AudioSendStream* send_stream) override;

  AudioReceiveStream* CreateAudioReceiveStream(
      const AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(AudioReceiveStream* receive_stream) override;

  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) override;
  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config,
      std::unique_ptr<FecController> fec_controller) override;
  void DestroyVideoSendStream(VideoSendStream* send_stream) override;

  VideoReceiveStream* CreateVideoReceiveStream(
      VideoReceiveStream::Config configuration) override;
  void DestroyVideoReceiveStream(VideoReceiveStream* receive_stream) override;

  FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config& config) override;
  void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) override;

  PacketReceiver* Receiver() override;

  RtpTransportControllerSendInterface* GetTransportControllerSend() override;

  Stats GetStats() const override;

  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override;

  void SignalChannelNetworkState(MediaType media, NetworkState state) override;

  void OnTransportOverheadChanged(MediaType media,
                                  int transport_overhead_per_packet) override;

  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

 protected:
  // Implements Transport.
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;

  bool SendRtcp(const uint8_t* packet, size_t length) override;

  // Implements PacketReceiver.
  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) override;

 private:
  // Overrides Module.
  int64_t TimeUntilNextProcess() override;
  void Process() override;

  Clock* const clock_;
  const std::unique_ptr<Call> call_;
  const rtc::CriticalSection config_lock_;

  const std::vector<DefaultNetworkSimulationConfig> send_configs_;
  const std::unique_ptr<ProcessThread> send_process_thread_;
  size_t send_config_index_ RTC_GUARDED_BY(config_lock_);
  int64_t send_config_start_time_ms_ RTC_GUARDED_BY(config_lock_);
  SimulatedNetwork* send_simulated_network_ RTC_GUARDED_BY(config_lock_);
  std::unique_ptr<FakeNetworkPipe> send_pipe_;
  size_t num_send_streams_;

  const std::vector<DefaultNetworkSimulationConfig> receive_configs_;
  size_t receive_config_index_ RTC_GUARDED_BY(config_lock_);
  int64_t receive_config_start_time_ms_ RTC_GUARDED_BY(config_lock_);
  SimulatedNetwork* receive_simulated_network_ RTC_GUARDED_BY(config_lock_);
  std::unique_ptr<FakeNetworkPipe> receive_pipe_;
};

}  // namespace webrtc

#endif  // CALL_DEGRADED_CALL_H_
