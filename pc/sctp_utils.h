/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_UTILS_H_
#define PC_SCTP_UTILS_H_

#include <string>

#include "api/data_channel_interface.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"  // For SSLRole
#include "rtc_base/system/no_unique_address.h"

namespace rtc {
class CopyOnWriteBuffer;
}  // namespace rtc

namespace webrtc {
struct DataChannelInit;

// Wraps the `uint16_t` sctp stream id value and does range checking.
// The class interface is `int` based to ease with DataChannelInit
// compatibility.
class SctpSid {
 public:
  SctpSid();
  explicit SctpSid(int id);
  explicit SctpSid(const SctpSid& sid);

  // Returns `true` if a valid id is contained, in the range of
  // kMinSctpSid - kSpecMaxSctpSid ([0..0xffff]). Note that this
  // is different than having `kMaxSctpSid` as the upper bound, which is
  // the limit that is internally used by `SctpSidAllocator`. Sid values may
  // be assigned to `SctpSid` outside of `SctpSidAllocator` and have a higher
  // id value than supplied by `SctpSidAllocator`, yet is still valid.
  bool IsValid() const;

  rtc::SSLRole role() const;
  int value() const;
  void reset();

  SctpSid& operator=(const SctpSid& sid);
  bool operator==(const SctpSid& sid) const;
  bool operator<(const SctpSid& sid) const;
  bool operator!=(const SctpSid& sid) const { return !(operator==(sid)); }

 private:
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;
  absl::optional<uint16_t> id_ RTC_GUARDED_BY(thread_checker_);
};

// Read the message type and return true if it's an OPEN message.
bool IsOpenMessage(const rtc::CopyOnWriteBuffer& payload);

bool ParseDataChannelOpenMessage(const rtc::CopyOnWriteBuffer& payload,
                                 std::string* label,
                                 DataChannelInit* config);

bool ParseDataChannelOpenAckMessage(const rtc::CopyOnWriteBuffer& payload);

bool WriteDataChannelOpenMessage(const std::string& label,
                                 const std::string& protocol,
                                 absl::optional<Priority> priority,
                                 bool ordered,
                                 absl::optional<int> max_retransmits,
                                 absl::optional<int> max_retransmit_time,
                                 rtc::CopyOnWriteBuffer* payload);
bool WriteDataChannelOpenMessage(const std::string& label,
                                 const DataChannelInit& config,
                                 rtc::CopyOnWriteBuffer* payload);
void WriteDataChannelOpenAckMessage(rtc::CopyOnWriteBuffer* payload);

}  // namespace webrtc

#endif  // PC_SCTP_UTILS_H_
