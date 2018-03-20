/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_DIRECT_TRANSPORT_H_
#define TEST_DIRECT_TRANSPORT_H_

#include <assert.h>

#include <memory>

#include "api/call/transport.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;
class PacketReceiver;

namespace test {

// Objects of this class are expected to be allocated and destroyed  on the
// same task-queue - the one that's passed in via the constructor.
class DirectTransport : public Transport {
 public:
  DirectTransport(rtc::TaskQueue* task_queue,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);

  DirectTransport(rtc::TaskQueue* task_queue,
                  const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);

  DirectTransport(rtc::TaskQueue* task_queue,
                  const FakeNetworkPipe::Config& config,
                  Call* send_call,
                  std::unique_ptr<Demuxer> demuxer);

  DirectTransport(rtc::TaskQueue* task_queue,
                  std::unique_ptr<FakeNetworkPipe> pipe,
                  Call* send_call);

  ~DirectTransport() override;

  void SetConfig(const FakeNetworkPipe::Config& config);

  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

  int GetAverageDelayMs();

 private:
  void SendPackets();
  void Start();

  class SendPacketsTask : public rtc::QueuedTask {
   public:
    SendPacketsTask(DirectTransport* transport) : transport_(transport) {}

    void set_should_run(bool should_run) {
      RTC_DCHECK_CALLED_SEQUENTIALLY(&main_sequence_);
      rtc::CritScope lock(&lock_);
      should_run_ = should_run;
    }

    bool SelfDestructIfQueued() {
      RTC_DCHECK_CALLED_SEQUENTIALLY(&main_sequence_);
      rtc::CritScope lock(&lock_);
      RTC_DCHECK(!self_destruct_);
      self_destruct_ = is_queued_;
      return is_queued_;
    }

    void raise_is_queued() {
      // RTC_DCHECK_CALLED_SEQUENTIALLY(&main_sequence_);
      rtc::CritScope lock(&lock_);
      RTC_DCHECK(!is_queued_);
      is_queued_ = true;
    }

   private:
    bool Run() override {
      rtc::CritScope lock(&lock_);
      is_queued_ = false;
      if (should_run_ && !self_destruct_)
        transport_->SendPackets();
      return self_destruct_;
    }

    rtc::SequencedTaskChecker main_sequence_;
    DirectTransport* const transport_;
    rtc::CriticalSection lock_;
    bool self_destruct_ = false;
    bool should_run_ = true;
    bool is_queued_ = false;
  };

  Call* const send_call_;
  Clock* const clock_;

  rtc::TaskQueue* const task_queue_;

  std::unique_ptr<SendPacketsTask> send_packets_;
  std::unique_ptr<FakeNetworkPipe> fake_network_;

  rtc::SequencedTaskChecker sequence_checker_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_DIRECT_TRANSPORT_H_
