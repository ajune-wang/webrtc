/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_

#include <vector>

#include "api/videosinkinterface.h"
#include "api/videosourceinterface.h"
#include "api/video_codecs/video_encoder.h"
#include "call/video_config.h"
#include "call/video_send_stream.h"

namespace webrtc {

// TODO(nisse): Move full declaration to api/.
class VideoBitrateAllocationObserver;

class VideoStreamEncoderInterface : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  // Interface for receiving encoded video frames and notifications about
  // configuration changes.
  class EncoderSink : public EncodedImageCallback {
   public:
    virtual void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        int min_transmit_bitrate_bps) = 0;
  };
  virtual void SetSource(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const VideoSendStream::DegradationPreference& degradation_preference) = 0;
  virtual void SetSink(EncoderSink* sink, bool rotation_applied) = 0;

  virtual void SetStartBitrate(int start_bitrate_bps) = 0;
  virtual void SendKeyFrame() = 0;
  virtual void OnBitrateUpdated(uint32_t bitrate_bps,
                                uint8_t fraction_lost,
                                int64_t round_trip_time_ms) = 0;

  virtual void SetBitrateObserver(
      VideoBitrateAllocationObserver* bitrate_observer) = 0;
  virtual void ConfigureEncoder(VideoEncoderConfig config,
                                size_t max_data_payload_length) = 0;
  virtual void Stop() = 0;

 protected:
  ~VideoStreamEncoderInterface() = default;
};
}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_
