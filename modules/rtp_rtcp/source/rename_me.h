/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RECOVERY_REQUEST_ADAPTER_H_
#define MODULES_RTP_RTCP_SOURCE_RECOVERY_REQUEST_ADAPTER_H_

#include <deque>

#include "absl/types/optional.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// TODO(eladalon): 1. Add documentation. 2. Improve naming scheme.
class RecoveryRequestAdapter final {
 public:
  RecoveryRequestAdapter();
  ~RecoveryRequestAdapter();

  struct Value {
    Value() = default;

    Value(uint32_t rtp_timestamp, bool is_first, bool is_last)
        : rtp_timestamp(rtp_timestamp), is_first(is_first), is_last(is_last) {}

    bool operator==(const Value& other) const {
      return rtp_timestamp == other.rtp_timestamp &&
             is_first == other.is_first && is_last == other.is_last;
    }

    uint32_t rtp_timestamp;
    bool is_first;
    bool is_last;
  };

  void RecordNewAssociation(uint16_t key, Value value);

  absl::optional<Value> GetValue(uint16_t key) const;

 private:
  struct Association {
    Association(uint16_t key, Value value) : key(key), value(value) {}

    uint16_t key;
    Value value;
  };

  rtc::CriticalSection cs_;

  // The odd order propety (AheadOf) would be problematic with a map,
  // so we use a deque instead.
  std::deque<Association> associations_ RTC_GUARDED_BY(cs_);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RECOVERY_REQUEST_ADAPTER_H_
