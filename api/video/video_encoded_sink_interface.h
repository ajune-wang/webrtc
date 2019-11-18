/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_ENCODED_SINK_INTERFACE_H_
#define API_VIDEO_VIDEO_ENCODED_SINK_INTERFACE_H_

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/color_space.h"
#include "api/video/video_codec_type.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// Interface handling consumption of encoded video frame buffers.
class VideoEncodedSinkInterface {
 public:
  // Interface for accessing elements of the encoded frame.
  class FrameBuffer : public rtc::RefCountInterface {
   public:
    // Encoded resolution in pixels
    struct EncodedResolution {
      unsigned width;
      unsigned height;
    };

    // Returns a span of the bitstream data.
    virtual rtc::ArrayView<const uint8_t> data() const = 0;

    // Returns the colorspace of the encoded frame, or nullptr if not present
    virtual const webrtc::ColorSpace* color_space() const = 0;

    // Returns the codec of the encoded frame
    virtual VideoCodecType codec() const = 0;

    // Returns wether the encoded frame is a keyframe
    virtual bool is_key_frame() const = 0;

    // Returns the frame's encoded resolution. May be 0x0 if the frame
    // doesn't contain resolution information
    virtual EncodedResolution resolution() const = 0;

    // Returns the render time in milliseconds
    virtual int64_t render_time() const = 0;
  };

  virtual ~VideoEncodedSinkInterface() = default;

  virtual void OnEncodedFrame(rtc::scoped_refptr<FrameBuffer> frame) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_ENCODED_SINK_INTERFACE_H_
