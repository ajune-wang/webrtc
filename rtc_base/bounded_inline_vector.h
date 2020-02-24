/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BOUNDED_INLINE_VECTOR_H_
#define RTC_BASE_BOUNDED_INLINE_VECTOR_H_

#include <stdint.h>
#include <limits>
#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

namespace bounded_inline_vector_impl {

template <bool...>
struct BoolPack;

// Tests if all its parameters (x0, x1, ..., xn) are true. The implementation
// checks whether (x0, x1, ..., xn, true) == (true, x0, x1, ..., xn), which is
// true iff true == x0 && x0 == x1 && x1 == x2 ... && xn-1 == xn && xn == true.
template <bool... Bs>
using AllTrue = std::is_same<BoolPack<Bs..., true>, BoolPack<true, Bs...>>;

template <class To, class... Froms>
using AllConvertible = AllTrue<std::is_convertible<Froms, To>::value...>;

}  // namespace bounded_inline_vector_impl

// A small std::vector-like type whose capacity is a compile-time constant. It
// stores all data inline and never heap allocates (beyond what its element type
// requires). Trying to grow it beyond its constant capacity is an error.
//
// TODO(kwiberg): Comparison operators.
// TODO(kwiberg): Methods for adding and deleting elements.
// TODO(kwiberg): Optimize for trivial T.
template <typename T, int Capacity>
class BoundedInlineVector {
  static_assert(!std::is_const<T>::value, "T may not be const");
  static_assert(Capacity > 0, "Capacity must be strictly positive");
  static_assert(Capacity <= std::numeric_limits<uint8_t>::max(),
                "Capacity must fit in 8 bits");

 public:
  using value_type = T;
  using const_iterator = const T*;

  BoundedInlineVector() : size_(0) {}

  template <
      typename... Ts,
      typename std::enable_if<
          bounded_inline_vector_impl::AllConvertible<T, Ts...>::value>::type* =
          nullptr>
  explicit BoundedInlineVector(Ts&&... elements)
      : size_(sizeof...(Ts)), data_{std::forward<Ts>(elements)...} {
    static_assert(sizeof...(Ts) <= Capacity, "");
  }

  template <int OtherCapacity>
  BoundedInlineVector(const BoundedInlineVector<T, OtherCapacity>& other)
      : size_(CopyElementsFrom(other)) {}

  template <int OtherCapacity>
  BoundedInlineVector(BoundedInlineVector<T, OtherCapacity>&& other)
      : size_(MoveElementsFrom(std::move(other))) {}

  template <int OtherCapacity>
  BoundedInlineVector& operator=(
      const BoundedInlineVector<T, OtherCapacity>& other) {
    DestroyElements();
    size_ = CopyElementsFrom(other);
    return *this;
  }

  template <int OtherCapacity>
  BoundedInlineVector& operator=(
      BoundedInlineVector<T, OtherCapacity>&& other) {
    DestroyElements();
    size_ = MoveElementsFrom(std::move(other));
    return *this;
  }

  ~BoundedInlineVector() { DestroyElements(); }

  bool empty() const { return size_ == 0; }
  int size() const { return size_; }
  constexpr int capacity() const { return Capacity; }

  const T* cdata() const { return data_; }
  const T* data() const { return data_; }
  T* data() { return data_; }

  const T& operator[](size_t index) const {
    RTC_DCHECK_GE(index, 0);
    RTC_DCHECK_LT(index, size_);
    return data_[index];
  }
  T& operator[](size_t index) {
    RTC_DCHECK_GE(index, 0);
    RTC_DCHECK_LT(index, size_);
    return data_[index];
  }

  T* begin() { return data_; }
  T* end() { return data_ + size_; }
  const T* begin() const { return data_; }
  const T* end() const { return data_ + size_; }
  const T* cbegin() const { return data_; }
  const T* cend() const { return data_ + size_; }

 private:
  template <typename C>
  uint8_t CopyElementsFrom(const C& other) {
    int next_index = 0;
    for (const auto& e : other) {
      RTC_DCHECK_LT(next_index, Capacity);
      ::new (&data_[next_index]) T(e);  // Placement new, because we construct a
                                        // new object in uninitialized memory.
      ++next_index;
    }
    const auto size = rtc::dchecked_cast<uint8_t>(next_index);
    RTC_DCHECK_LE(size, Capacity);
    return size;
  }

  template <typename C>
  uint8_t MoveElementsFrom(C&& other) {
    int next_index = 0;
    for (auto& e : other) {
      RTC_DCHECK_LT(next_index, Capacity);
      ::new (&data_[next_index])
          T(std::move(e));  // Placement new, because we construct a
                            // new object in uninitialized memory.
      ++next_index;
    }
    const auto size = rtc::dchecked_cast<uint8_t>(next_index);
    RTC_DCHECK_LE(size, Capacity);
    return size;
  }

  void DestroyElements() {
    for (int i = 0; i < size_; ++i) {
      data_[i].~T();
    }
  }

  uint8_t size_;
  union {
    // Since this array is in a union, we get to construct and destroy it
    // manually.
    T data_[Capacity];
  };
};

}  // namespace webrtc

#endif  // RTC_BASE_BOUNDED_INLINE_VECTOR_H_
