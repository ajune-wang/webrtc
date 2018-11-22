/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_TEST_MOCK_VIDEO_BITRATE_ALLOCATOR_H_
#define WEBRTC_API_TEST_MOCK_VIDEO_BITRATE_ALLOCATOR_H_

#include "api/video/video_bitrate_allocator.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoBitrateAllocator : public webrtc::VideoBitrateAllocator {
  MOCK_METHOD2(GetAllocation,
               VideoBitrateAllocation(uint32_t total_bitrate,
                                      uint32_t framerate));
  MOCK_METHOD1(GetPreferredBitrateBps, uint32_t(uint32_t framerate));
};

}  // namespace webrtc

#endif  // WEBRTC_API_TEST_MOCK_VIDEO_BITRATE_ALLOCATOR_H_
