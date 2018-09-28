/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <utility>

#include "call/degraded_call.h"

#include "absl/memory/memory.h"

namespace webrtc {
DegradedCall::DegradedCall(
    std::unique_ptr<Call> call,
    std::vector<DefaultNetworkSimulationConfig> send_configs,
    std::vector<DefaultNetworkSimulationConfig> receive_configs)
    : clock_(Clock::GetRealTimeClock()),
      call_(std::move(call)),
      send_configs_(send_configs),
      send_process_thread_(ProcessThread::Create("DegradedSendThread")),
      send_config_index_(0),
      send_config_start_time_ms_(clock_->TimeInMilliseconds()),
      send_simulated_network_(nullptr),
      num_send_streams_(0),
      receive_configs_(receive_configs),
      receive_config_index_(0),
      receive_config_start_time_ms_(clock_->TimeInMilliseconds()) {
  if (!receive_configs_.empty()) {
    auto network = absl::make_unique<SimulatedNetwork>(
        receive_configs_[receive_config_index_]);
    receive_simulated_network_ = network.get();
    receive_pipe_ =
        absl::make_unique<webrtc::FakeNetworkPipe>(clock_, std::move(network));
    receive_pipe_->SetReceiver(call_->Receiver());
  }
  send_process_thread_->Start();
  send_process_thread_->RegisterModule(this, RTC_FROM_HERE);
}

DegradedCall::~DegradedCall() {
  if (send_pipe_) {
    send_process_thread_->DeRegisterModule(send_pipe_.get());
  }
  send_process_thread_->DeRegisterModule(this);

  send_process_thread_->Stop();
}

AudioSendStream* DegradedCall::CreateAudioSendStream(
    const AudioSendStream::Config& config) {
  return call_->CreateAudioSendStream(config);
}

void DegradedCall::DestroyAudioSendStream(AudioSendStream* send_stream) {
  call_->DestroyAudioSendStream(send_stream);
}

AudioReceiveStream* DegradedCall::CreateAudioReceiveStream(
    const AudioReceiveStream::Config& config) {
  return call_->CreateAudioReceiveStream(config);
}

void DegradedCall::DestroyAudioReceiveStream(
    AudioReceiveStream* receive_stream) {
  call_->DestroyAudioReceiveStream(receive_stream);
}

VideoSendStream* DegradedCall::CreateVideoSendStream(
    VideoSendStream::Config config,
    VideoEncoderConfig encoder_config) {
  if (!send_configs_.empty() && !send_pipe_) {
    rtc::CritScope cs(&config_lock_);
    send_config_index_ = 0;
    send_config_start_time_ms_ = clock_->TimeInMilliseconds();
    auto network =
        absl::make_unique<SimulatedNetwork>(send_configs_[send_config_index_]);
    send_simulated_network_ = network.get();
    send_pipe_ = absl::make_unique<FakeNetworkPipe>(clock_, std::move(network),
                                                    config.send_transport);
    config.send_transport = this;
    send_process_thread_->RegisterModule(send_pipe_.get(), RTC_FROM_HERE);
  }
  ++num_send_streams_;
  return call_->CreateVideoSendStream(std::move(config),
                                      std::move(encoder_config));
}

VideoSendStream* DegradedCall::CreateVideoSendStream(
    VideoSendStream::Config config,
    VideoEncoderConfig encoder_config,
    std::unique_ptr<FecController> fec_controller) {
  if (!send_configs_.empty() && !send_pipe_) {
    rtc::CritScope cs(&config_lock_);
    send_config_index_ = 0;
    send_config_start_time_ms_ = clock_->TimeInMilliseconds();
    auto network =
        absl::make_unique<SimulatedNetwork>(send_configs_[send_config_index_]);
    send_simulated_network_ = network.get();
    send_pipe_ = absl::make_unique<FakeNetworkPipe>(clock_, std::move(network),
                                                    config.send_transport);
    config.send_transport = this;
    send_process_thread_->RegisterModule(send_pipe_.get(), RTC_FROM_HERE);
  }
  ++num_send_streams_;
  return call_->CreateVideoSendStream(
      std::move(config), std::move(encoder_config), std::move(fec_controller));
}

void DegradedCall::DestroyVideoSendStream(VideoSendStream* send_stream) {
  call_->DestroyVideoSendStream(send_stream);
  if (send_pipe_ && num_send_streams_ > 0) {
    --num_send_streams_;
    if (num_send_streams_ == 0) {
      send_process_thread_->DeRegisterModule(send_pipe_.get());
      rtc::CritScope cs(&config_lock_);
      send_pipe_.reset();
      send_simulated_network_ = nullptr;
    }
  }
}

VideoReceiveStream* DegradedCall::CreateVideoReceiveStream(
    VideoReceiveStream::Config configuration) {
  return call_->CreateVideoReceiveStream(std::move(configuration));
}

void DegradedCall::DestroyVideoReceiveStream(
    VideoReceiveStream* receive_stream) {
  call_->DestroyVideoReceiveStream(receive_stream);
}

FlexfecReceiveStream* DegradedCall::CreateFlexfecReceiveStream(
    const FlexfecReceiveStream::Config& config) {
  return call_->CreateFlexfecReceiveStream(config);
}

void DegradedCall::DestroyFlexfecReceiveStream(
    FlexfecReceiveStream* receive_stream) {
  call_->DestroyFlexfecReceiveStream(receive_stream);
}

PacketReceiver* DegradedCall::Receiver() {
  if (!receive_configs_.empty()) {
    return this;
  }
  return call_->Receiver();
}

RtpTransportControllerSendInterface*
DegradedCall::GetTransportControllerSend() {
  return call_->GetTransportControllerSend();
}

Call::Stats DegradedCall::GetStats() const {
  return call_->GetStats();
}

void DegradedCall::SetBitrateAllocationStrategy(
    std::unique_ptr<rtc::BitrateAllocationStrategy>
        bitrate_allocation_strategy) {
  call_->SetBitrateAllocationStrategy(std::move(bitrate_allocation_strategy));
}

void DegradedCall::SignalChannelNetworkState(MediaType media,
                                             NetworkState state) {
  call_->SignalChannelNetworkState(media, state);
}

void DegradedCall::OnTransportOverheadChanged(
    MediaType media,
    int transport_overhead_per_packet) {
  call_->OnTransportOverheadChanged(media, transport_overhead_per_packet);
}

void DegradedCall::OnSentPacket(const rtc::SentPacket& sent_packet) {
  if (!send_configs_.empty()) {
    // If we have a degraded send-transport, we have already notified call
    // about the supposed network send time. Discard the actual network send
    // time in order to properly fool the BWE.
    return;
  }
  call_->OnSentPacket(sent_packet);
}

bool DegradedCall::SendRtp(const uint8_t* packet,
                           size_t length,
                           const PacketOptions& options) {
  // A call here comes from the RTP stack (probably pacer). We intercept it and
  // put it in the fake network pipe instead, but report to Call that is has
  // been sent, so that the bandwidth estimator sees the delay we add.
  send_pipe_->SendRtp(packet, length, options);
  if (options.packet_id != -1) {
    rtc::SentPacket packet_info;
    packet_info.packet_id = options.packet_id;
    packet_info.send_time_ms = clock_->TimeInMilliseconds();
    call_->OnSentPacket(packet_info);
  }
  return true;
}

bool DegradedCall::SendRtcp(const uint8_t* packet, size_t length) {
  send_pipe_->SendRtcp(packet, length);
  return true;
}

PacketReceiver::DeliveryStatus DegradedCall::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    int64_t packet_time_us) {
  PacketReceiver::DeliveryStatus status = receive_pipe_->DeliverPacket(
      media_type, std::move(packet), packet_time_us);
  // This is not optimal, but there are many places where there are thread
  // checks that fail if we're not using the worker thread call into this
  // method. If we want to fix this we probably need a task queue to do handover
  // of all overriden methods, which feels like overikill for the current use
  // case.
  // By just having this thread call out via the Process() method we work around
  // that, with the tradeoff that a non-zero delay may become a little larger
  // than anticipated at very low packet rates.
  receive_pipe_->Process();
  return status;
}

int64_t DegradedCall::TimeUntilNextProcess() {
  rtc::CritScope cs(&config_lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t time_until_next = 1000000;

  if (!send_configs_.empty()) {
    int64_t duration = send_configs_[send_config_index_].config_durations_ms;
    if (duration > 0) {
      time_until_next = std::min(
          time_until_next, send_config_start_time_ms_ + duration - now_ms);
    }
  }

  if (!receive_configs_.empty()) {
    int64_t duration =
        receive_configs_[receive_config_index_].config_durations_ms;
    if (duration > 0) {
      time_until_next = std::min(
          time_until_next, receive_config_start_time_ms_ + duration - now_ms);
    }
  }

  return std::max(0l, time_until_next);
}

void DegradedCall::Process() {
  rtc::CritScope cs(&config_lock_);
  int64_t now_ms = clock_->TimeInMilliseconds();

  if (!send_configs_.empty()) {
    int64_t duration = send_configs_[send_config_index_].config_durations_ms;
    if (duration > 0 && now_ms >= send_config_start_time_ms_ + duration) {
      send_config_index_ = (send_config_index_ + 1) % send_configs_.size();
      send_config_start_time_ms_ += duration;
      if (send_simulated_network_) {
        send_simulated_network_->SetConfig(send_configs_[send_config_index_]);
      }
    }
  }

  if (!receive_configs_.empty()) {
    int64_t duration =
        receive_configs_[receive_config_index_].config_durations_ms;
    if (duration > 0 && now_ms >= receive_config_start_time_ms_ + duration) {
      receive_config_index_ =
          (receive_config_index_ + 1) % receive_configs_.size();
      receive_config_start_time_ms_ += duration;
      if (receive_simulated_network_) {
        receive_simulated_network_->SetConfig(
            receive_configs_[send_config_index_]);
      }
    }
  }
}

}  // namespace webrtc
