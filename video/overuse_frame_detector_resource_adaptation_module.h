/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
#define VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/video_encoder_config.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video/video_stream_encoder_observer.h"
#include "call/adaptation/resource_adaptation_module_interface.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

class OveruseFrameDetectorResourceAdaptationModule
    : public ResourceAdaptationModuleInterface,
      // Should these be protected or privately inherrited?
      public AdaptationObserverInterface {
 public:
 OveruseFrameDetectorResourceAdaptationModule();
  ~OveruseFrameDetectorResourceAdaptationModule() override;

  // TODO(hbos): Make private or protected.
  class AdaptCounter final {
   public:
    AdaptCounter();
    ~AdaptCounter();

    // Get number of adaptation downscales for |reason|.
    VideoStreamEncoderObserver::AdaptationSteps Counts(int reason) const;

    std::string ToString() const;

    void IncrementFramerate(int reason);
    void IncrementResolution(int reason);
    void DecrementFramerate(int reason);
    void DecrementResolution(int reason);
    void DecrementFramerate(int reason, int cur_fps);

    // Gets the total number of downgrades (for all adapt reasons).
    int FramerateCount() const;
    int ResolutionCount() const;

    // Gets the total number of downgrades for |reason|.
    int FramerateCount(int reason) const;
    int ResolutionCount(int reason) const;
    int TotalCount(int reason) const;

   private:
    std::string ToString(const std::vector<int>& counters) const;
    int Count(const std::vector<int>& counters) const;
    void MoveCount(std::vector<int>* counters, int from_reason);

    // Degradation counters holding number of framerate/resolution reductions
    // per adapt reason.
    std::vector<int> fps_counters_;
    std::vector<int> resolution_counters_;
  };

 protected:
  // AdaptationObserverInterface implementation.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

 private:
  // TODO(hbos): RTC_GUARDED_BY(&encoder_queue_); everywhere...
  class VideoSourceProxy;

  class VideoFrameInfo {
   public:
    VideoFrameInfo(int width, int height, bool is_texture)
        : width(width), height(height), is_texture(is_texture) {}
    int width;
    int height;
    bool is_texture;
    int pixel_count() const { return width * height; }
  };

  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;
  };

  void UpdateAdaptationStats(AdaptReason reason);
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptReason reason);

  // TODO(hbos): Remove...
  DegradationPreference EffectiveDegradataionPreference();
  AdaptCounter& GetAdaptCounter();
  const AdaptCounter& GetConstAdaptCounter();

  // DUMMY
  bool CanAdaptUpResolution(int pixels, uint32_t bitrate_bps) const;

  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const DegradationPreference, AdaptCounter> adapt_counters_;
  // Set depending on degradation preferences.
  DegradationPreference degradation_preference_;
  const BalancedDegradationSettings balanced_settings_;
  DegradationPreference effective_degradation_preference_;

  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  absl::optional<AdaptationRequest> last_adaptation_request_;

  // TODO(hbos): Initialize, reconfigure???
  VideoStreamEncoderObserver* const encoder_stats_observer_ = nullptr;
  const std::unique_ptr<OveruseFrameDetector> overuse_detector_;
  int max_framerate_ = 0;
  const std::unique_ptr<VideoSourceProxy> source_proxy_;
  uint32_t encoder_start_bitrate_bps_ = 0;
  VideoEncoderConfig encoder_config_;
  absl::optional<VideoFrameInfo> last_frame_info_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<QualityScaler> quality_scaler_;
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
