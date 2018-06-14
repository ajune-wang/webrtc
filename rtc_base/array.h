/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ARRAY_H_
#define RTC_BASE_ARRAY_H_

#include <utility>

#include "rtc_base/checks.h"

namespace rtc {

template <typename T, size_t S>
class Array {
 public:
  Array() : size_(0) {}

  ~Array() { clear(); }

  void push_back(const T& value) {
    RTC_CHECK_LT(size_, S);
    array_[size_++] = value;
  }

  void push_back(T&& value) {
    RTC_CHECK_LT(size_, S);
    array_[size_++] = std::move(value);
  }

  template <typename... A>
  void emplace_back(A&&... args) {
    RTC_CHECK_LT(size_, S);
    new (&array_[size_++]) T(std::forward<A>(args)...);
  }

  void clear() {
    for (size_t i = 0; i < size_; ++i)
      array_[i].~T();
    size_ = 0;
  }

  T& operator[](size_t index) {
    RTC_CHECK_LT(index, size_);
    return array_[index];
  }

  const T& operator[](size_t index) const {
    RTC_CHECK_LT(index, size_);
    return array_[index];
  }

  size_t capacity() const { return S; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  bool full() const { return size_ == S; }

 private:
  size_t size_;

  // Using a union to avoid having to initilize |array_| on construction.
  union {
    T array_[S];
  };
};

}  // namespace rtc

#endif  // RTC_BASE_ARRAY_H_
