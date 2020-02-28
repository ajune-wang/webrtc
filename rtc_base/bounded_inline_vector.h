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
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"

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

// Initializes part of an uninitialized array. Unlike normal array
// initialization, does not zero the remaining array elements.
template <typename T, typename... Us>
void InitializeElements(T* data, Us&&... elements) {
  ::new (data) T[sizeof...(Us)]{
      std::forward<Us>(elements)...};  // Placement new, because we construct a
                                       // new object in uninitialized memory.
}

// Copies from source to uninitialized destination.
template <typename T>
void CopyElements(const T* src_data, int src_size, T* dst_data, int* dst_size) {
  if /*constexpr*/ (std::is_trivially_copy_constructible<T>::value) {
    std::memcpy(dst_data, src_data, src_size * sizeof(T));
  } else {
    std::uninitialized_copy_n(src_data, src_size, dst_data);
  }
  *dst_size = src_size;
}

// Moves from source to uninitialized destination.
template <typename T>
void MoveElements(T* src_data, int src_size, T* dst_data, int* dst_size) {
  if /*constexpr*/ (std::is_trivially_move_constructible<T>::value) {
    std::memcpy(dst_data, src_data, src_size * sizeof(T));
  } else {
    for (int i = 0; i < src_size; ++i) {
      ::new (&dst_data[i])
          T(std::move(src_data[i]));  // Placement new, because we construct a
                                      // new object in uninitialized memory.
    }
  }
  *dst_size = src_size;
}

// Destroys elements, leaving them uninitialized.
template <typename T>
void DestroyElements(T* data, int size) {
  if /*constexpr*/ (!std::is_trivially_destructible<T>::value) {
    for (int i = 0; i < size; ++i) {
      data[i].~T();
    }
  }
}

// If elements are trivial and the total capacity is at most this many bytes,
// copy everything instead of just the elements that are in use; this is more
// efficient, and makes BoundedInlineVector trivially copyable.
static constexpr int kSmallSize = 64;

template <typename T,
          int Capacity,
          bool Trivial = std::is_trivial<T>::value,
          bool Small = (sizeof(T) * Capacity <= kSmallSize)>
struct Storage {
  static_assert(!std::is_trivial<T>::value, "");

  template <
      typename... Ts,
      typename std::enable_if<AllConvertible<T, Ts...>::value>::type* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage& other) {
    CopyElements(other.data, other.size, data, &size);
  }

  Storage(Storage&& other) {
    MoveElements(other.data, other.size, data, &size);
  }

  Storage& operator=(const Storage& other) {
    DestroyElements(data, size);
    CopyElements(other.data, other.size, data, &size);
  }

  Storage& operator=(Storage&& other) {
    DestroyElements(data, size);
    MoveElements(other.data, other.size, data, &size);
  }

  ~Storage() { DestroyElements(data, size); }

  int size;
  union {
    // Since this array is in a union, we get to construct and destroy it
    // manually.
    T data[Capacity];
  };
};

template <typename T, int Capacity>
struct Storage<T, Capacity, /*Trivial=*/true, /*Small=*/true> {
  static_assert(std::is_trivial<T>::value, "");
  static_assert(sizeof(T) * Capacity <= kSmallSize, "");

  template <
      typename... Ts,
      typename std::enable_if<AllConvertible<T, Ts...>::value>::type* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage&) = default;
  Storage& operator=(const Storage&) = default;
  ~Storage() = default;

  int size;
  T data[Capacity];
};

template <typename T, int Capacity>
struct Storage<T, Capacity, /*Trivial=*/true, /*Small=*/false> {
  static_assert(std::is_trivial<T>::value, "");
  static_assert(sizeof(T) * Capacity > kSmallSize, "");

  template <
      typename... Ts,
      typename std::enable_if<AllConvertible<T, Ts...>::value>::type* = nullptr>
  explicit Storage(Ts&&... elements) : size(sizeof...(Ts)) {
    InitializeElements(data, std::forward<Ts>(elements)...);
  }

  Storage(const Storage& other) : size(other.size) {
    std::memcpy(data, other.data, other.size * sizeof(T));
  }

  Storage& operator=(const Storage& other) {
    size = other.size;
    std::memcpy(data, other.data, other.size * sizeof(T));
  }

  ~Storage() = default;

  int size;
  union {
    T data[Capacity];
  };
};

}  // namespace bounded_inline_vector_impl

// A small std::vector-like type whose capacity is a compile-time constant. It
// stores all data inline and never heap allocates (beyond what its element type
// requires). Trying to grow it beyond its constant capacity is an error.
//
// TODO(kwiberg): Comparison operators.
// TODO(kwiberg): Methods for adding and deleting elements.
template <typename T, int Capacity>
class BoundedInlineVector {
  static_assert(!std::is_const<T>::value, "T may not be const");
  static_assert(Capacity > 0, "Capacity must be strictly positive");
  static_assert(Capacity <= std::numeric_limits<uint8_t>::max(),
                "Capacity must fit in 8 bits");

 public:
  using value_type = T;
  using const_iterator = const T*;

  BoundedInlineVector() = default;
  BoundedInlineVector(const BoundedInlineVector&) = default;
  BoundedInlineVector(BoundedInlineVector&&) = default;
  BoundedInlineVector& operator=(const BoundedInlineVector&) = default;
  BoundedInlineVector& operator=(BoundedInlineVector&&) = default;
  ~BoundedInlineVector() = default;

  template <
      typename... Ts,
      typename std::enable_if<
          bounded_inline_vector_impl::AllConvertible<T, Ts...>::value>::type* =
          nullptr>
  explicit BoundedInlineVector(Ts&&... elements)
      : storage_{std::forward<Ts>(elements)...} {
    static_assert(sizeof...(Ts) <= Capacity, "");
  }

  template <int OtherCapacity,
            typename std::enable_if<OtherCapacity != Capacity>::type* = nullptr>
  BoundedInlineVector(const BoundedInlineVector<T, OtherCapacity>& other) {
    RTC_DCHECK_LE(other.size(), Capacity);
    bounded_inline_vector_impl::CopyElements(other.data(), other.size(),
                                             storage_.data, &storage_.size);
  }

  template <int OtherCapacity,
            typename std::enable_if<OtherCapacity != Capacity>::type* = nullptr>
  BoundedInlineVector(BoundedInlineVector<T, OtherCapacity>&& other) {
    RTC_DCHECK_LE(other.size(), Capacity);
    bounded_inline_vector_impl::MoveElements(other.data(), other.size(),
                                             storage_.data, &storage_.size);
  }

  template <int OtherCapacity,
            typename std::enable_if<OtherCapacity != Capacity>::type* = nullptr>
  BoundedInlineVector& operator=(
      const BoundedInlineVector<T, OtherCapacity>& other) {
    bounded_inline_vector_impl::DestroyElements(storage_.data, storage_.size);
    RTC_DCHECK_LE(other.size(), Capacity);
    bounded_inline_vector_impl::CopyElements(other.data(), other.size(),
                                             storage_.data, &storage_.size);
    return *this;
  }

  template <int OtherCapacity,
            typename std::enable_if<OtherCapacity != Capacity>::type* = nullptr>
  BoundedInlineVector& operator=(
      BoundedInlineVector<T, OtherCapacity>&& other) {
    bounded_inline_vector_impl::DestroyElements(storage_.data, storage_.size);
    RTC_DCHECK_LE(other.size(), Capacity);
    bounded_inline_vector_impl::MoveElements(other.data(), other.size(),
                                             storage_.data, &storage_.size);
    return *this;
  }

  bool empty() const { return storage_.size == 0; }
  int size() const { return storage_.size; }
  constexpr int capacity() const { return Capacity; }

  const T* cdata() const { return storage_.data; }
  const T* data() const { return storage_.data; }
  T* data() { return storage_.data; }

  const T& operator[](int index) const {
    RTC_DCHECK_GE(index, 0);
    RTC_DCHECK_LT(index, storage_.size);
    return storage_.data[index];
  }
  T& operator[](int index) {
    RTC_DCHECK_GE(index, 0);
    RTC_DCHECK_LT(index, storage_.size);
    return storage_.data[index];
  }

  T* begin() { return storage_.data; }
  T* end() { return storage_.data + storage_.size; }
  const T* begin() const { return storage_.data; }
  const T* end() const { return storage_.data + storage_.size; }
  const T* cbegin() const { return storage_.data; }
  const T* cend() const { return storage_.data + storage_.size; }

 private:
  bounded_inline_vector_impl::Storage<T, Capacity> storage_;
};

}  // namespace webrtc

#endif  // RTC_BASE_BOUNDED_INLINE_VECTOR_H_
