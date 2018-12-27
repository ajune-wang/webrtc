/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "media/engine/web_rtc_video_capturer.h"
#include "media/engine/web_rtc_video_device_capturer_factory.h"

namespace cricket {

std::unique_ptr<VideoCapturer> WebRtcVideoDeviceCapturerFactory::Create(
    const Device& device) {
#ifdef HAVE_WEBRTC_VIDEO
  std::unique_ptr<WebRtcVideoCapturer> capturer(new WebRtcVideoCapturer());
  if (!capturer->Init(device)) {
    return std::unique_ptr<VideoCapturer>();
  }
  return std::move(capturer);
#else
  return std::unique_ptr<VideoCapturer>();
#endif
}

}  // namespace cricket
