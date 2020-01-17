/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REF_COUNTED_OBJECT_H_
#define RTC_BASE_REF_COUNTED_OBJECT_H_

#include <type_traits>
#include <utility>

#include "rtc_base/constructor_magic.h"
#include "rtc_base/ref_count.h"

namespace rtc {

template <class T, typename Enable = void>
class RefCountedObject;

template <class T>
class RefCountedObject<
    T,
    typename std::enable_if<std::is_base_of<RefCountInterface, T>::value>::type>
    : public T {
 public:
  RefCountedObject() {}

  template <class P0>
  explicit RefCountedObject(P0&& p0) : T(std::forward<P0>(p0)) {}

  template <class P0, class P1, class... Args>
  RefCountedObject(P0&& p0, P1&& p1, Args&&... args)
      : T(std::forward<P0>(p0),
          std::forward<P1>(p1),
          std::forward<Args>(args)...) {}

 protected:
  RTC_DISALLOW_COPY_AND_ASSIGN(RefCountedObject);
};

template <class T>
class RefCountedObject<T,
                       typename std::enable_if<
                           !std::is_base_of<RefCountInterface, T>::value>::type>
    : public RefCountInterface, public T {
 public:
  RefCountedObject() {}

  template <class P0>
  explicit RefCountedObject(P0&& p0) : T(std::forward<P0>(p0)) {}

  template <class P0, class P1, class... Args>
  RefCountedObject(P0&& p0, P1&& p1, Args&&... args)
      : T(std::forward<P0>(p0),
          std::forward<P1>(p1),
          std::forward<Args>(args)...) {}

 protected:
  RTC_DISALLOW_COPY_AND_ASSIGN(RefCountedObject);
};

}  // namespace rtc

#endif  // RTC_BASE_REF_COUNTED_OBJECT_H_
