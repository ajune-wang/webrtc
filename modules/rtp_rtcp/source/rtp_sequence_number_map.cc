/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"

#include <algorithm>
#include <iterator>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {
namespace {
using Info = RtpSequenceNumberMap::Info;
}  // namespace

RtpSequenceNumberMap::RtpSequenceNumberMap(size_t max_entries)
    : max_entries_(max_entries) {
  RTC_DCHECK_GT(max_entries_, 4);  // See code paring down to |max_entries_|.
  RTC_DCHECK_LE(max_entries_, (1 << 15) + 1);
  // The DCHECKs below show that ((1 << 15) + 1) is the maximum number of
  // entries that can be held and still have the newest AheadOf() the oldest.
  RTC_DCHECK(
      AheadOf(static_cast<uint16_t>((1 << 15)), static_cast<uint16_t>(0)));
  RTC_DCHECK(
      AheadOf(static_cast<uint16_t>(0), static_cast<uint16_t>((1 << 15) + 1)));
}

RtpSequenceNumberMap::~RtpSequenceNumberMap() = default;

void RtpSequenceNumberMap::Insert(uint16_t sequence_number, Info info) {
  rtc::CritScope cs(&cs_);

  RTC_DCHECK(associations_.size() < 2 ||
             AheadOf(associations_.back().sequence_number,
                     associations_.front().sequence_number));

  if (associations_.empty()) {
    associations_.emplace_back(sequence_number, info);
    return;
  }

  if (sequence_number == associations_.front().sequence_number ||
      sequence_number == associations_.back().sequence_number ||
      (AheadOf(sequence_number, associations_.front().sequence_number) &&
       AheadOf(associations_.back().sequence_number, sequence_number))) {
    // The sequence number has wrapped around and is within the range
    // currently held by |associations_| - we should invalidate all entries.
    associations_.clear();
    associations_.emplace_back(sequence_number, info);
    return;
  }

  std::deque<Association>::iterator erase_to = associations_.begin();

  RTC_DCHECK_LE(associations_.size(), max_entries_);
  if (associations_.size() == max_entries_) {
    // Delete half the entries if the maximum would be exceeded.
    // (Deleting a single element each time would be too inefficient.)
    const size_t new_size = 3 * max_entries_ / 4;
    erase_to = std::next(erase_to, max_entries_ - new_size);
  }

  // It is guaranteed that |associations_| can be split into two partitions,
  // either partition possibly empty, such that:
  // * In the first partition, all elements are AheadOf the new element.
  //   This is the partition of the obsolete elements.
  // * In the second partition, the new element is AheadOf all the elements.
  //   The elements of this partition may stay.
  const Association new_association(sequence_number, info);
  auto cmp = [](const Association& a, const Association& b) {
    return AheadOf(a.sequence_number, b.sequence_number);
  };
  RTC_DCHECK(erase_to != associations_.end());
  erase_to =
      std::lower_bound(erase_to, associations_.end(), new_association, cmp);
  associations_.erase(associations_.begin(), erase_to);

  associations_.push_back(new_association);

  RTC_DCHECK(associations_.size() == 1 ||
             AheadOf(associations_.back().sequence_number,
                     associations_.front().sequence_number));
}

absl::optional<Info> RtpSequenceNumberMap::Get(uint16_t sequence_number) const {
  rtc::CritScope cs(&cs_);

  // To make the binary search easier to understand, we use the fact that
  // adding a constant offset to all elements, as well as to the searched
  // element, does not change the relative ordering. This way, we can find
  // an offset that would make all of the elements strictly ascending according
  // to normal integer comparison.
  // Finding such an offset is easy - the offset that would map the oldest
  // element to 0 would serve this purpose.

  if (associations_.empty()) {
    return absl::nullopt;
  }

  const uint16_t offset =
      static_cast<uint16_t>(0) - associations_.front().sequence_number;

  auto cmp = [offset](const Association& a, const Association& b) {
    return static_cast<uint16_t>(a.sequence_number + offset) <
           static_cast<uint16_t>(b.sequence_number + offset);
  };
  const auto elem = std::lower_bound(associations_.begin(), associations_.end(),
                                     Association(sequence_number), cmp);

  return elem != associations_.end() && elem->sequence_number == sequence_number
             ? absl::optional<Info>(elem->info)
             : absl::nullopt;
}

size_t RtpSequenceNumberMap::AssociationCountForTesting() const {
  rtc::CritScope cs(&cs_);
  return associations_.size();
}

}  // namespace webrtc
