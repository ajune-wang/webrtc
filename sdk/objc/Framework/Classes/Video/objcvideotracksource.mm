/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/Framework/Classes/Video/objcvideotracksource.h"

#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#include "rtc_base/logging.h"
#include "api/video/i420_buffer.h"
#include "sdk/objc/Framework/Native/src/objc_frame_buffer.h"

namespace webrtc {

ObjcVideoTrackSource::ObjcVideoTrackSource() {}

void ObjcVideoTrackSource::OnOutputFormatRequest(int width, int height, int fps) {
  cricket::VideoFormat format(width, height, cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter()->OnOutputFormatRequest(format);
}

void ObjcVideoTrackSource::OnCapturedFrame(RTCVideoFrame* frame) {
  const int64_t timestamp_us = frame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
  const int64_t translated_timestamp_us =
      timestamp_aligner_.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  if (!AdaptFrame(frame.width, frame.height, timestamp_us, &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y)) {
    return;
  }
    if (frame.width ==1280 && frame.height==720) {
        adapted_width = 640;
        adapted_height = 480;
        crop_x = 160;
        crop_y = 0;
        crop_width = 960;
        crop_height = 720;
    }

  rtc::scoped_refptr<VideoFrameBuffer> buffer;
  rtc::scoped_refptr<VideoFrameBuffer> depth_buffer;
  /*if (adapted_width == frame.width && adapted_height == frame.height) {
    // No adaption - optimized path.
    buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
  } else if ([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
    // Adapted CVPixelBuffer frame.
    RTCCVPixelBuffer *rtcPixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
    buffer = new rtc::RefCountedObject<ObjCFrameBuffer>([[RTCCVPixelBuffer alloc]
        initWithPixelBuffer:rtcPixelBuffer.pixelBuffer
               adaptedWidth:adapted_width
              adaptedHeight:adapted_height
                  cropWidth:crop_width
                 cropHeight:crop_height
                      cropX:crop_x + rtcPixelBuffer.cropX
                      cropY:crop_y + rtcPixelBuffer.cropY]);
  } else {
    // Adapted I420 frame.
    // TODO(magjed): Optimize this I420 path.
   
  }*/

    if (false) {
        rtc::scoped_refptr<I420Buffer> i420_buffer = I420Buffer::Create(adapted_width, adapted_height);
        buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
        i420_buffer->CropAndScaleFrom(*buffer->ToI420(), crop_x, crop_y, crop_width, crop_height);
        buffer = i420_buffer;

    } else {

      rtc::scoped_refptr<I420ABuffer> i420a_buffer = I420ABuffer::Create(adapted_width, adapted_height);
      buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.buffer);
      depth_buffer = new rtc::RefCountedObject<ObjCFrameBuffer>(frame.depth_buffer);
      i420a_buffer->CropAndScaleFrom(*buffer->ToI420(), crop_x, crop_y, crop_width, crop_height);
      crop_x = 0;
      crop_y = 0;
      crop_width = depth_buffer->width();
      crop_height = depth_buffer->height();
      rtc::scoped_refptr<I420BufferInterface> depth_i420 = depth_buffer->ToI420();
      i420a_buffer->CropAndScaleFromYAsA(*depth_i420, crop_x, crop_y, crop_width, crop_height);
      buffer = i420a_buffer;
    }

  // Applying rotation is only supported for legacy reasons and performance is
  // not critical here.
  webrtc::VideoRotation rotation = static_cast<webrtc::VideoRotation>(frame.rotation);
  /*if (apply_rotation() && rotation != kVideoRotation_0) {
    buffer = I420Buffer::Rotate(*buffer->ToI420(), rotation);
    rotation = kVideoRotation_0;
  }*/

  OnFrame(webrtc::VideoFrame(buffer, rotation, translated_timestamp_us));
}

}  // namespace webrtc
