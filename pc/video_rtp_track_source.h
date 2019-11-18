/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEO_RTP_TRACK_SOURCE_H_
#define PC_VIDEO_RTP_TRACK_SOURCE_H_

#include <vector>

#include "api/video/video_encoded_sink_interface.h"
#include "media/base/video_broadcaster.h"
#include "pc/video_track_source.h"
#include "rtc_base/callback.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

// Video track source in use by VideoRtpReceiver
class VideoRtpTrackSource : public VideoTrackSource {
 public:
  class Callback {
   public:
    virtual ~Callback() = default;

    // Called when a keyframe should be generated
    virtual void OnGenerateKeyFrame() = 0;

    // Called when the implementor should eventually start to serve encoded
    // frames using BroadcastEncodedFrameBuffer.
    // The implementor should cause a keyframe to be eventually generated.
    virtual void OnEncodedSinkEnabled(bool enable) = 0;
  };

  explicit VideoRtpTrackSource(Callback* callback);

  // Call before the object implementing Callback finishes it's destructor. No
  // more callbacks will be fired after completion.
  void ClearCallback();

  // Call to broadcast an encoded FrameBuffer to registered sinks
  void BroadcastEncodedFrameBuffer(
      rtc::scoped_refptr<VideoEncodedSinkInterface::FrameBuffer> frame_buffer);

  // VideoTrackSource
  rtc::VideoSourceInterface<VideoFrame>* source() override;
  rtc::VideoSinkInterface<VideoFrame>* sink();

  // VideoTrackSourceInterface
  bool SupportsEncodedOutput() const override;
  void GenerateKeyFrame() override;
  void AddEncodedSink(VideoEncodedSinkInterface* sink) override;
  void RemoveEncodedSink(VideoEncodedSinkInterface* sink) override;

 private:
  // |broadcaster_| is needed since the decoder can only handle one sink.
  // It might be better if the decoder can handle multiple sinks and consider
  // the VideoSinkWants.
  rtc::VideoBroadcaster broadcaster_;
  rtc::CriticalSection mu_;
  std::vector<VideoEncodedSinkInterface*> encoded_sinks_ RTC_GUARDED_BY(mu_);
  Callback* callback_ RTC_GUARDED_BY(mu_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoRtpTrackSource);
};

}  // namespace webrtc

#endif  // PC_VIDEO_RTP_TRACK_SOURCE_H_
