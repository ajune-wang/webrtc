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

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "rtc_base/checks.h"
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
  using OutputT = Output;

  virtual ~Future() = default;

  virtual poll_t<Output> Poll(Context*) = 0;
};

template <typename Output>
class BoxedFuture final : public Future<Output> {
 public:
  explicit BoxedFuture(std::unique_ptr<Future<Output>> future)
      : future_(std::move(future)) {}

  // Future implementation.
  poll_t<Output> Poll(Context* context) override {
    RTC_DCHECK(future_);
    return future_->Poll(context);
  }

  std::unique_ptr<Future<Output>> Release() { return std::move(future_); }

 private:
  std::unique_ptr<Future<Output>> future_;
};

template <typename T, typename... Args>
BoxedFuture<typename T::OutputT> MakeBoxedFuture(Args&&... args) {
  std::unique_ptr<Future<typename T::OutputT>> future =
      std::make_unique<T>(std::forward<Args>(args)...);
  return BoxedFuture<typename T::OutputT>(std::move(future));
}

struct Void {};

}  // namespace webrtc

#endif  // RTC_BASE_FUTURES_FUTURE_H_
