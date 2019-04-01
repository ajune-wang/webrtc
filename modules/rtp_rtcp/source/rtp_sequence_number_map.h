/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SEQUENCE_NUMBER_MAP_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SEQUENCE_NUMBER_MAP_H_

#include <deque>

#include "absl/types/optional.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Records the association of RTP sequence numbers to timestamps and to whether
// the packet was first and/or last in the frame.
//
// 1. Thread-safe.
// 2. Limits number of entries. Whenever |max_entries| is about to be exceeded,
//    the size is reduced to (max_entries / 2).
// 3. RTP sequence numbers wrap around relatively infrequently.
//    This class therefore only remembers at most the last 2^15 RTP packets,
//    so that the newest packet's sequence number is still AheadOf the oldest
//    packet's sequence number.
// 4. If a media frame is split into several RTP packets, all of those packets'
//    sequence numbers will be mapped to the first one's sequence number.
class RtpSequenceNumberMap final {
 public:
  explicit RtpSequenceNumberMap(size_t max_entries);
  ~RtpSequenceNumberMap();

  struct Info final {
    Info(uint32_t timestamp, bool is_first, bool is_last)
        : timestamp(timestamp), is_first(is_first), is_last(is_last) {}

    bool operator==(const Info& other) const {
      return timestamp == other.timestamp && is_first == other.is_first &&
             is_last == other.is_last;
    }

    uint32_t timestamp;
    bool is_first;
    bool is_last;
  };

  void Insert(uint16_t sequence_number, Info info);

  absl::optional<Info> Get(uint16_t sequence_number) const;

  size_t AssociationCountForTesting() const;

 private:
  struct Association {
    explicit Association(uint16_t sequence_number)
        : Association(sequence_number, Info(0, false, false)) {}

    Association(uint16_t sequence_number, Info info)
        : sequence_number(sequence_number), info(info) {}

    uint16_t sequence_number;
    Info info;
  };

  const size_t max_entries_;

  rtc::CriticalSection cs_;

  // The non-transitivity of AheadOf() would be problematic with a map,
  // so we use a deque instead.
  std::deque<Association> associations_ RTC_GUARDED_BY(cs_);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SEQUENCE_NUMBER_MAP_H_
