/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ZERO_HERTZ_ENCODER_ADAPTER_H_
#define VIDEO_ZERO_HERTZ_ENCODER_ADAPTER_H_

#include <memory>

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// A sink adapter implementing zero-hertz min fps encoding mode.
// With the exception of construction & destruction which has to happen on the
// same sequence, this class is thread-safe because three different execution
// contexts call into it.
class ZeroHertzEncoderAdapterInterface
    : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  // Callback interface used to inform instance owners.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Called from OnFrame (and hence, the context that calls into OnFrame)
    // whenever zero-hertz frame mode deactivated.
    virtual void OnZeroHertzModeDeactivated() = 0;
  };

  // Factory function creating a production instance. Deletion of the returned
  // instance needs to happen on the same sequence that Create() was called on.
  static std::unique_ptr<ZeroHertzEncoderAdapterInterface> Create();

  // Sets up sink and callback. This method must be called before the rest of
  // the API is used.
  virtual void Initialize(rtc::VideoSinkInterface<VideoFrame>& sink,
                          Callback& callback) = 0;

  // Pass true in |enabled| when suitable constraints have been received
  // enabling zero-hertz mode. False otherwise.
  virtual void SetEnabledByConstraints(bool enabled) = 0;

  // Pass true in |enabled| when the content type allows.
  virtual void SetEnabledByContentType(bool enabled) = 0;
};

}  // namespace webrtc

#endif  // VIDEO_ZERO_HERTZ_ENCODER_ADAPTER_H_
