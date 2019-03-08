/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_H_
#define API_TEST_NETEQ_SIMULATOR_H_

#include <stdint.h>
#include <map>
#include <vector>

namespace webrtc {
namespace test {

class NetEqSimulator {
 public:
  virtual ~NetEqSimulator() = default;

  enum class Action { kNormal, kExpand, kAccelerate, kPreemptiveExpand };

  struct RtpPacketInfo {
    // Arrival time in microseconds.
    int64_t arrival_time_us = 0;
    // RTP sequence number
    int sequence_number = 0;
    // Indicate if this is a padding packet or not.
    bool padding = false;
    // Indicate if this packet contains DTX.
    bool dtx_packet = false;
    // Amount of audio in this packet in samples.
    int audio_content_samples = 0;
    // The RTP timestamp from the header of the packet. This corresponds to the
    // number of the first sample in the packet.
    uint32_t rtp_timestamp = 0;
  };

  struct NetEqState {
    NetEqState();
    NetEqState(const NetEqState& other);
    ~NetEqState();
    // Current simulation time in microseconds.
    int64_t current_simulation_time_us = 0;
    // The sum of the packet buffer and sync buffer delay.
    int current_delay_ms = 0;
    // An indicator that the packet buffer has been flushed since the last
    // GetAudio event.
    bool packet_buffer_flushed = false;
    // Information about the packet that arrived since the last GetAudio event.
    std::vector<RtpPacketInfo> arrived_packets;
    // The current buffer size in samples.
    int buffer_size_samples = 0;
    // The sequence number of the last decoded packet.
    int last_decoded_timestamp = 0;
    // Total samples sent to sound card.
    int64_t total_playout_samples = 0;
    // Total discarded samples due to late arrivals and buffer flushes.
    int64_t total_discarded_samples = 0;
    // Total concealed samples due to buffer underruns.
    int64_t total_concealed_samples = 0;
    // Total concealed samples during non-silent audio playout due to buffer
    // underruns.
    int64_t total_concealed_nonsilent_samples = 0;
    // Total removed samples due to increasing the playout speed.
    int64_t total_accelerated_samples = 0;
    // Total added samples due to decreasing the playout speed.
    int64_t total_decelerated_samples = 0;
    // The audio sample rate in hertz.
    int sample_rate_hz = 0;
  };

  // Runs the simulation until we hit the next GetAudio event. If the simulation
  // has reached the end, this function will return false.
  virtual bool RunToNextGetAudio() = 0;

  // Set the next action to be taken by NetEq. This will override any action
  // that NetEq would normally decide to take.
  virtual void SetNextAction(Action next_operation) = 0;

  // Get the current state of NetEq.
  virtual NetEqState GetNetEqState() = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETEQ_SIMULATOR_H_
