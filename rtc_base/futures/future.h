/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FUTURES_FUTURE_H_
#define RTC_BASE_FUTURES_FUTURE_H_

#include <utility>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class Waker : public rtc::RefCountInterface {
 public:
  virtual ~Waker() = default;

  virtual void WakeByRef() = 0;
};

struct Context final {
 public:
  static Context FromWaker(rtc::scoped_refptr<Waker> waker) {
    return Context(std::move(waker));
  }

  rtc::scoped_refptr<Waker> waker() { return waker_; }

 private:
  explicit Context(rtc::scoped_refptr<Waker> waker)
      : waker_(std::move(waker)) {}

  rtc::scoped_refptr<Waker> waker_;
};

template <typename Output>
using poll_t = absl::optional<Output>;

template <typename Output>
class Future {
 public:
  virtual ~Future() = default;

  virtual poll_t<Output> Poll(Context*) = 0;
};

struct Void {};

}  // namespace webrtc

#endif  // RTC_BASE_FUTURES_FUTURE_H_
