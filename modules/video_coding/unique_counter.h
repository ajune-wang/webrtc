/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_UNIQUE_COUNTER_H_
#define MODULES_VIDEO_CODING_UNIQUE_COUNTER_H_

#include <cstdint>
#include <set>
#include <vector>

namespace webrtc {

// Counts number of uniquly seen values.
class UniqueCounter {
 public:
  UniqueCounter();
  UniqueCounter(const UniqueCounter&) = delete;
  UniqueCounter& operator=(const UniqueCounter&) = delete;
  ~UniqueCounter() = default;

  void Add(uint32_t value);
  // Returns number of different |value| passed to the UniqueCounter.
  int GetUniqueSeen() const { return unique_seen_; }

 private:
  int unique_seen_ = 0;
  // Stores several last seen unique values for quick search.
  std::set<uint32_t> index_;
  // The same unique values in the circular buffer in the insertion order.
  std::vector<uint32_t> latest_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UNIQUE_COUNTER_H_
