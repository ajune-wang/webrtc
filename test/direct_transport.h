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

#include <memory>

#include "api/call/transport.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/simulated_network.h"
#include "call/call.h"
#include "call/simulated_packet_receiver.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread_annotations.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {

class PacketReceiver;

namespace test {
class Demuxer {
 public:
  explicit Demuxer(const std::map<uint8_t, MediaType>& payload_type_map);
  ~Demuxer() = default;
  MediaType GetMediaType(const uint8_t* packet_data,
                         const size_t packet_length) const;
  const std::map<uint8_t, MediaType> payload_type_map_;
  RTC_DISALLOW_COPY_AND_ASSIGN(Demuxer);
};

// Objects of this class are expected to be allocated and destroyed  on the
// same task-queue - the one that's passed in via the constructor.
class DirectTransport : public Transport {
 public:
  DirectTransport(TaskQueueBase* task_queue,
                  std::unique_ptr<SimulatedPacketReceiverInterface> pipe,
                  Call* send_call,
                  const std::map<uint8_t, MediaType>& payload_type_map);

  ~DirectTransport() override;

  // TODO(holmer): Look into moving this to the constructor.
  virtual void SetReceiver(PacketReceiver* receiver);

  bool SendRtp(const uint8_t* data,
               size_t length,
               const PacketOptions& options) override;
  bool SendRtcp(const uint8_t* data, size_t length) override;

  int GetAverageDelayMs();

 private:
  class ProcessFakeNetworkTask;

  void SendPacket(const uint8_t* data, size_t length);
  void Start();

  Call* const send_call_;
  // owned by the fake_network_task_.
  SimulatedPacketReceiverInterface* const fake_network_;
  // Ownership shared between between |this| and |task_queue_|
  // fake_network_task has two flags to note which ownerships are active.
  ProcessFakeNetworkTask* const fake_network_task_;

  const Demuxer demuxer_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_DIRECT_TRANSPORT_H_
