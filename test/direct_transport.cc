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

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "rtc_base/time_utils.h"
#include "test/rtp_header_parser.h"

namespace webrtc {
namespace test {

class DirectTransport::ProcessFakeNetworkTask final : public QueuedTask {
 public:
  ProcessFakeNetworkTask(
      TaskQueueBase* task_queue,
      std::unique_ptr<SimulatedPacketReceiverInterface> fake_network)
      : task_queue_(task_queue), fake_network_(std::move(fake_network)) {
    RTC_DCHECK(task_queue_);
    RTC_DCHECK(fake_network_);
  }
  ~ProcessFakeNetworkTask() override = default;

  bool Run() override {
    RTC_DCHECK_RUN_ON(task_queue_);
    {
      rtc::CritScope lock(&state_mutex_);
      RTC_DCHECK(owned_by_task_queue_);
      if (!owned_by_direct_transport_) {
        // Direct transport is gone, fake_network is deleted.
        // The only thing left to do is to self-destruct.
        return true;
      }
    }
    fake_network_->Process();
    absl::optional<int64_t> delay_ms = fake_network_->TimeUntilNextProcess();
    if (delay_ms == absl::nullopt) {
      rtc::CritScope lock(&state_mutex_);
      RTC_DCHECK(owned_by_task_queue_);
      owned_by_task_queue_ = false;
      RTC_DCHECK(owned_by_direct_transport_);
      return false;
    }
    task_queue_->PostDelayedTask(absl::WrapUnique(this), *delay_ms);
    // still owned by the task queue, so do not delete this.
    return false;
  }

  void ReleaseDirectTransportOwnership() {
    RTC_DCHECK_RUN_ON(task_queue_);
    fake_network_ = nullptr;
    {
      rtc::CritScope lock(&state_mutex_);
      RTC_DCHECK(owned_by_direct_transport_);
      owned_by_direct_transport_ = false;
      if (owned_by_task_queue_) {
        // Postpone destruction. Let task queue delete this task.
        return;
      }
    }
    delete this;
  }

  void MayBePostToTaskQueue() {
    // Note: this method might be called from arbitrary thread/queue.
    {
      // Do not post if already on the task queue.
      rtc::CritScope lock(&state_mutex_);
      if (owned_by_task_queue_)
        return;
      owned_by_task_queue_ = true;
    }
    absl::optional<int64_t> delay_ms = fake_network_->TimeUntilNextProcess();
    if (delay_ms == absl::nullopt) {
      // Do not post if fake_network doesn't request processing.
      rtc::CritScope lock(&state_mutex_);
      RTC_DCHECK(owned_by_task_queue_);
      owned_by_task_queue_ = false;
      return;
    }

    task_queue_->PostDelayedTask(absl::WrapUnique(this), *delay_ms);
  }

 private:
  TaskQueueBase* const task_queue_;
  std::unique_ptr<SimulatedPacketReceiverInterface> fake_network_;

  rtc::CriticalSection state_mutex_;
  bool owned_by_task_queue_ RTC_GUARDED_BY(state_mutex_) = false;
  bool owned_by_direct_transport_ RTC_GUARDED_BY(state_mutex_) = true;
};

Demuxer::Demuxer(const std::map<uint8_t, MediaType>& payload_type_map)
    : payload_type_map_(payload_type_map) {}

MediaType Demuxer::GetMediaType(const uint8_t* packet_data,
                                const size_t packet_length) const {
  if (!RtpHeaderParser::IsRtcp(packet_data, packet_length)) {
    RTC_CHECK_GE(packet_length, 2);
    const uint8_t payload_type = packet_data[1] & 0x7f;
    std::map<uint8_t, MediaType>::const_iterator it =
        payload_type_map_.find(payload_type);
    RTC_CHECK(it != payload_type_map_.end())
        << "payload type " << static_cast<int>(payload_type) << " unknown.";
    return it->second;
  }
  return MediaType::ANY;
}

DirectTransport::DirectTransport(
    TaskQueueBase* task_queue,
    std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : send_call_(send_call),
      fake_network_(pipe.get()),
      fake_network_task_(
          new ProcessFakeNetworkTask(task_queue, std::move(pipe))),
      demuxer_(payload_type_map) {
  Start();
}

DirectTransport::~DirectTransport() {
  // If fake_network_task_ was posted to the task queue, this call will not
  // delete it.
  fake_network_task_->ReleaseDirectTransportOwnership();
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  fake_network_->SetReceiver(receiver);
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id, rtc::TimeMillis());
    sent_packet.info.included_in_feedback = options.included_in_feedback;
    sent_packet.info.included_in_allocation = options.included_in_allocation;
    sent_packet.info.packet_size_bytes = length;
    sent_packet.info.packet_type = rtc::PacketType::kData;
    send_call_->OnSentPacket(sent_packet);
  }
  SendPacket(data, length);
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  SendPacket(data, length);
  return true;
}

void DirectTransport::SendPacket(const uint8_t* data, size_t length) {
  MediaType media_type = demuxer_.GetMediaType(data, length);
  int64_t send_time_us = rtc::TimeMicros();
  fake_network_->DeliverPacket(media_type, rtc::CopyOnWriteBuffer(data, length),
                               send_time_us);
  fake_network_task_->MayBePostToTaskQueue();
}

int DirectTransport::GetAverageDelayMs() {
  return fake_network_->AverageDelay();
}

void DirectTransport::Start() {
  if (send_call_) {
    send_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    send_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }
}

}  // namespace test
}  // namespace webrtc
