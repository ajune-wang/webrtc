/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_TEST_MOCK_BITRATE_ALLOCATOR_H_
#define CALL_TEST_MOCK_BITRATE_ALLOCATOR_H_

#include <string>

#include "call/bitrate_allocator.h"
#include "test/gmock.h"

namespace webrtc {
class MockBitrateAllocator : public BitrateAllocatorInterface {
 public:
  MOCK_METHOD8(AddObserver,
               void(BitrateAllocatorObserver*,
                    uint32_t,
                    uint32_t,
                    uint32_t,
                    bool,
                    std::string,
                    double,
                    bool));
  MOCK_METHOD1(RemoveObserver, void(BitrateAllocatorObserver*));
  MOCK_METHOD1(GetStartBitrate, int(BitrateAllocatorObserver*));
};
}  // namespace webrtc
#endif  // CALL_TEST_MOCK_BITRATE_ALLOCATOR_H_
