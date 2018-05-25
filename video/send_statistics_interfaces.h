/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_SEND_STATISTICS_INTERFACES_H_
#define VIDEO_SEND_STATISTICS_INTERFACES_H_

#include <vector>

#include "api/video_codecs/video_encoder_config.h"

namespace webrtc {

// Broken out into a base class, with public inheritance below, only to ease
// unit testing of the internal class OveruseFrameDetector.
class CpuOveruseMetricsObserver {
 public:
  virtual ~CpuOveruseMetricsObserver() = default;
  virtual void OnEncodedFrameTimeMeasured(int encode_duration_ms,
                                          int encode_usage_percent) = 0;
};

class EncoderStatsObserver : public CpuOveruseMetricsObserver {
 public:
  // Number of resolution and framerate reductions (-1: disabled).
  struct AdaptCounts {
    int resolution = 0;
    int fps = 0;
  };

  virtual ~EncoderStatsObserver() = default;

  virtual void OnIncomingFrame(int width, int height) = 0;

  // TODO(nisse): Merge into one callback per encoded frame.
  using CpuOveruseMetricsObserver::OnEncodedFrameTimeMeasured;
  virtual void OnSendEncodedImage(const EncodedImage& encoded_image,
                                  const CodecSpecificInfo* codec_info) = 0;

  virtual void OnFrameDroppedBySource() = 0;
  virtual void OnFrameDroppedInEncoderQueue() = 0;
  virtual void OnFrameDroppedByEncoder() = 0;
  virtual void OnFrameDroppedByMediaOptimizations() = 0;

  // Used to indicate change in content type, which may require a change in
  // how stats are collected and set the configured preferred media bitrate.
  virtual void OnEncoderReconfigured(const VideoEncoderConfig& encoder_config,
                                     const std::vector<VideoStream>& streams,
                                     uint32_t preferred_bitrate_bps) = 0;

  virtual void SetAdaptationStats(const AdaptCounts& cpu_counts,
                                  const AdaptCounts& quality_counts) = 0;
  virtual void OnCpuAdaptationChanged(const AdaptCounts& cpu_counts,
                                      const AdaptCounts& quality_counts) = 0;
  virtual void OnQualityAdaptationChanged(
      const AdaptCounts& cpu_counts,
      const AdaptCounts& quality_counts) = 0;
  virtual void OnMinPixelLimitReached() = 0;
  virtual void OnInitialQualityResolutionAdaptDown() = 0;

  virtual void OnSuspendChange(bool is_suspended) = 0;

  // TODO(nisse): VideoStreamEncoder wants to query the stats, which makes this
  // not a pure observer. GetInputFrameRate is needed for the cpu adaptation, so
  // can be deleted of that responsibility is moved out to a VideoStreamAdaptor
  // class. GetSendFrameRate is passed to the VideoBitrateAllocator to produce
  // the needed for the the preferred_bitrate stat value, which appears unused.
  virtual int GetInputFrameRate() const = 0;
  virtual int GetSendFrameRate() const = 0;
};

}  // namespace webrtc
#endif  // VIDEO_SEND_STATISTICS_INTERFACES_H_
