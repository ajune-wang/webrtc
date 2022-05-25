/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_
#define RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_

#include <memory>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

template <typename T>
struct UniquePtrCreator {
 public:
  typedef T element_type;
  typedef typename std::unique_ptr<element_type> unique_pointer_type;

  unique_pointer_type operator()() const {
    return std::make_unique<element_type>();
  }

  template <typename Args>
  unique_pointer_type operator()(Args&& args) const {
    return std::make_unique<element_type>(std::forward<Args>(args));
  }

  template <typename... Args>
  unique_pointer_type operator()(Args&&... args) const {
    return std::make_unique<element_type>(std::forward<Args>(args)...);
  }
};

template <typename T>
struct FatalCreator {
 public:
  typedef T element_type;
  typedef typename std::unique_ptr<element_type> unique_pointer_type;

  unique_pointer_type operator()() const { RTC_FATAL(); }

  template <typename... Args>
  unique_pointer_type operator()(Args&&... args) const {
    RTC_FATAL();
  }

  template <typename... Args>
  unique_pointer_type operator()(const Args&... args) const {
    RTC_FATAL();
  }
};

// This template allows the instantiation of a pointer to Interface in such a
// way that if it is passed a null pointer, an object will be created, by using
// callable object |CreateFunc|, which will be deallocated when the pointer
// is deleted. By default, |std::unique_ptr| of |Interface| will be created.
// This behavior can be customized by using different |CreateFunc| template
// parameter.
template <typename Interface, typename CreateFunc = UniquePtrCreator<Interface>>
class AlwaysValidPointer {
 public:
  explicit AlwaysValidPointer(Interface* pointer)
      : owned_instance_(pointer ? nullptr : CreateFunc()()),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  template <typename... Args>
  explicit AlwaysValidPointer(Interface* pointer, Args&&... args)
      : owned_instance_(pointer ? nullptr : CreateFunc()(args...)),
        pointer_(pointer ? pointer : owned_instance_.get()) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) using |pointer|, without taking ownership
  // b) calling |function| and taking ownership of the result
  template <typename Func,
            typename std::enable_if<std::is_invocable<Func>::value,
                                    bool>::type = true>
  AlwaysValidPointer(Interface* pointer, Func function)
      : owned_instance_(pointer ? nullptr : function()),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) taking over ownership of |instance|
  // b) or fallback to |pointer|, without taking ownership.
  // c) or calling |CreateFunc|.
  explicit AlwaysValidPointer(std::unique_ptr<Interface> instance,
                              Interface* pointer = nullptr)
      : owned_instance_(instance
                            ? std::move(instance)
                            : (pointer == nullptr ? CreateFunc()() : nullptr)),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
    RTC_DCHECK(pointer_);
  }

  // Create a pointer by
  // a) taking over ownership of |instance|
  // b) or fallback to |pointer|, without taking ownership.
  // c) or |CreateFunc| (with forwarded args).
  template <typename... Args>
  AlwaysValidPointer(std::unique_ptr<Interface> instance,
                     Interface* pointer,
                     Args&&... args)
      : owned_instance_(instance
                            ? std::move(instance)
                            : (pointer == nullptr
                                   ? CreateFunc()(std::forward<Args>(args)...)
                                   : nullptr)),
        pointer_(owned_instance_ ? owned_instance_.get() : pointer) {
    RTC_DCHECK(pointer_);
  }

  explicit operator bool() const { return pointer_ != nullptr; }

  Interface* get() { return pointer_; }
  Interface* operator->() { return pointer_; }
  Interface& operator*() { return *pointer_; }

  Interface* get() const { return pointer_; }
  Interface* operator->() const { return pointer_; }
  Interface& operator*() const { return *pointer_; }

 private:
  const std::unique_ptr<Interface> owned_instance_;
  Interface* const pointer_;
};

// Helper namespace/structs for partial template specialization.
namespace detail {
template <typename Interface>
struct NoDefault {
  typedef AlwaysValidPointer<Interface, FatalCreator<Interface>> type;
};

template <typename Interface, typename Default>
struct WithDefault {
  typedef AlwaysValidPointer<Interface, UniquePtrCreator<Default>> type;
};

}  // namespace detail

// This partial template specialization calls |RTC_FATAL| if a pointer needs to
// be created inside an instance of this.
template <typename Interface>
using AlwaysValidPointerNoDefault = typename detail::NoDefault<Interface>::type;

// This partial template specialization works by internally creating an object
// of class |Default| type when necessary.
template <typename Interface, typename Default>
using AlwaysValidPointerWithDefault =
    typename detail::WithDefault<Interface, Default>::type;

template <typename T, typename U, typename V, typename W>
bool operator==(const AlwaysValidPointer<T, U>& a,
                const AlwaysValidPointer<V, W>& b) {
  return a.get() == b.get();
}

template <typename T, typename U, typename V, typename W>
bool operator!=(const AlwaysValidPointer<T, U>& a,
                const AlwaysValidPointer<V, W>& b) {
  return !(a == b);
}

template <typename T, typename U>
bool operator==(const AlwaysValidPointer<T, U>& a, std::nullptr_t) {
  return a.get() == nullptr;
}

template <typename T, typename U>
bool operator!=(const AlwaysValidPointer<T, U>& a, std::nullptr_t) {
  return !(a == nullptr);
}

template <typename T, typename U>
bool operator==(std::nullptr_t, const AlwaysValidPointer<T, U>& a) {
  return a.get() == nullptr;
}

template <typename T, typename U>
bool operator!=(std::nullptr_t, const AlwaysValidPointer<T, U>& a) {
  return !(a == nullptr);
}

// Comparison with raw pointer.
template <typename T, typename U, typename V>
bool operator==(const AlwaysValidPointer<T, U>& a, const V* b) {
  return a.get() == b;
}

template <typename T, typename U, typename V>
bool operator!=(const AlwaysValidPointer<T, U>& a, const V* b) {
  return !(a == b);
}

template <typename T, typename U, typename V>
bool operator==(const T* a, const AlwaysValidPointer<U, V>& b) {
  return a == b.get();
}

template <typename T, typename U, typename V>
bool operator!=(const T* a, const AlwaysValidPointer<U, V>& b) {
  return !(a == b);
}

}  // namespace webrtc

#endif  // RTC_BASE_MEMORY_ALWAYS_VALID_POINTER_H_
