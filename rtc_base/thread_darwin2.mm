/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/thread_darwin.h"

#import <Foundation/Foundation.h>

#include "rtc_base/checks.h"

namespace rtc {

void InitCocoaMultiThreading() {
  if ([NSThread isMultiThreaded] == NO) {
    // The sole purpose of this autorelease pool is to avoid a console
    // message on Leopard that tells us we're autoreleasing the thread
    // with no autorelease pool in place.
    @autoreleasepool {
      [NSThread detachNewThreadSelector:@selector(class)
                               toTarget:[NSObject class]
                             withObject:nil];
    }
  }

  RTC_DCHECK([NSThread isMultiThreaded]);
}

}  // namespace rtc
