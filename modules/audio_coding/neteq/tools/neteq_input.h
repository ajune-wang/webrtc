/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_

#include <algorithm>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "modules/audio_coding/neteq/tools/packet_source.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace test {

// Interface class for input to the NetEqTest class.
class NetEqInput {
 public:
  struct PacketData {
    PacketData();
    ~PacketData();
    std::string ToString() const;

    RTPHeader header;
    rtc::Buffer payload;
    int64_t time_ms;
  };

  struct SetMinimumDelay {
    SetMinimumDelay(int64_t timestamp_ms_in, int delay_ms_in)
        : timestamp_ms(timestamp_ms_in), delay_ms(delay_ms_in) {}
    int64_t timestamp_ms;
    int delay_ms;
  };

  struct GetAudio {
    explicit GetAudio(int64_t timestamp_ms_in)
        : timestamp_ms(timestamp_ms_in) {}
    int64_t timestamp_ms;
  };

  struct Event {
    bool Empty() const {
      return set_minimum_delay == nullptr && packet_data == nullptr &&
             audio_output == nullptr;
    }
    std::unique_ptr<SetMinimumDelay> set_minimum_delay;
    std::unique_ptr<PacketData> packet_data;
    std::unique_ptr<GetAudio> audio_output;
  };

  virtual ~NetEqInput() = default;

  virtual const Event& NextEvent() const = 0;

  virtual Event PopEvent() = 0;

  // Returns true if the source has come to an end. An implementation must
  // eventually return true from this method, or the test will end up in an
  // infinite loop.
  virtual bool ended() const = 0;

  // Returns the RTP header for the next packet, i.e., the packet that will be
  // delivered next by PopPacket().
  virtual absl::optional<RTPHeader> NextHeader() const = 0;

  // Returns the time (in ms) for the next event, or empty if both are out of
  // events.
  absl::optional<int64_t> NextEventTime() const {
    const Event& next_event = NextEvent();
    if (next_event.audio_output) {
      return next_event.audio_output->timestamp_ms;
    }
    if (next_event.packet_data) {
      return next_event.packet_data->time_ms;
    }
    if (next_event.set_minimum_delay) {
      return next_event.set_minimum_delay->timestamp_ms;
    }
    return absl::nullopt;
  }
};

// Wrapper class to impose a time limit on a NetEqInput object, typically
// another time limit than what the object itself provides. For example, an
// input taken from a file can be cut shorter by wrapping it in this class.
class TimeLimitedNetEqInput : public NetEqInput {
 public:
  TimeLimitedNetEqInput(std::unique_ptr<NetEqInput> input, int64_t duration_ms);
  ~TimeLimitedNetEqInput() override;
  const Event& NextEvent() const override;
  virtual Event PopEvent() override;
  bool ended() const override;
  absl::optional<RTPHeader> NextHeader() const override;

 private:
  void MaybeSetEnded();

  std::unique_ptr<NetEqInput> input_;
  const absl::optional<int64_t> start_time_ms_;
  const int64_t duration_ms_;
  bool ended_ = false;
  Event empty_event_;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_INPUT_H_
