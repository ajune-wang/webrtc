/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/direct_transport.h"

#include "call/call.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

DirectTransport::DirectTransport(
    rtc::TaskQueue* task_queue,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(task_queue,
                      FakeNetworkPipe::Config(),
                      send_call,
                      payload_type_map) {}

DirectTransport::DirectTransport(
    rtc::TaskQueue* task_queue,
    const FakeNetworkPipe::Config& config,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(
          task_queue,
          config,
          send_call,
          std::unique_ptr<Demuxer>(new DemuxerImpl(payload_type_map))) {}

DirectTransport::DirectTransport(rtc::TaskQueue* task_queue,
                                 const FakeNetworkPipe::Config& config,
                                 Call* send_call,
                                 std::unique_ptr<Demuxer> demuxer)
    : send_call_(send_call),
      clock_(Clock::GetRealTimeClock()),
      task_queue_(task_queue),
      fake_network_(rtc::MakeUnique<FakeNetworkPipe>(clock_,
                                                     config,
                                                     std::move(demuxer))) {
  Start();
}

DirectTransport::DirectTransport(rtc::TaskQueue* task_queue,
                                 std::unique_ptr<FakeNetworkPipe> pipe,
                                 Call* send_call)
    : send_call_(send_call),
      clock_(Clock::GetRealTimeClock()),
      task_queue_(task_queue),
      fake_network_(std::move(pipe)) {
  Start();
}

DirectTransport::~DirectTransport() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  // Constructor updates |next_scheduled_task_|, so it's guaranteed to
  // be initialized.
  // task_queue_->CancelTask(next_scheduled_task_);
  if (send_packets_ && send_packets_->SelfDestructIfQueued())
    send_packets_.release();
}

void DirectTransport::SetConfig(const FakeNetworkPipe::Config& config) {
  fake_network_->SetConfig(config);
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  fake_network_->SetReceiver(receiver);
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id,
                                clock_->TimeInMilliseconds());
    send_call_->OnSentPacket(sent_packet);
  }
  fake_network_->SendPacket(data, length);
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  fake_network_->SendPacket(data, length);
  return true;
}

int DirectTransport::GetAverageDelayMs() {
  return fake_network_->AverageDelay();
}

void DirectTransport::Start() {
  RTC_DCHECK(task_queue_);
  RTC_DCHECK(!send_packets_);

  if (send_call_) {
    send_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    send_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }

  send_packets_.reset(new SendPacketsTask(this));
  send_packets_->raise_is_queued();
  // SendPackets();
  task_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(send_packets_.get()));
}

void DirectTransport::SendPackets() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  fake_network_->Process();

  const int64_t delay_ms = fake_network_->TimeUntilNextProcess();
  // next_scheduled_task_ =
  send_packets_->raise_is_queued();
  task_queue_->PostDelayedTask(
      std::unique_ptr<rtc::QueuedTask>(send_packets_.get()), delay_ms);
}
}  // namespace test
}  // namespace webrtc
