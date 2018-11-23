/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/refcounter.h"

namespace webrtc {
namespace webrtc_impl {

RefCounter::RefCounter(int ref_count) : ref_count_(ref_count) {}

}  // namespace webrtc_impl
}  // namespace webrtc
