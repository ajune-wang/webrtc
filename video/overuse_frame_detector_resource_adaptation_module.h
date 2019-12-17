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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/resource_adaptation_module_interface.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

class OveruseFrameDetectorResourceAdaptationModule
    : public ResourceAdaptationModuleInterface,
      // Should these be protected or privately inherrited?
      public AdaptationObserverInterface {
 public:
  OveruseFrameDetectorResourceAdaptationModule(
      rtc::VideoSinkInterface<VideoFrame>* sink,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      VideoStreamEncoderObserver* encoder_stats_observer);
  ~OveruseFrameDetectorResourceAdaptationModule() override;

  void Initialize(rtc::TaskQueue* encoder_queue);
  DegradationPreference degradation_preference() const {
    return degradation_preference_;
  }

  // Input to the OveruseFrameDetector, which are required for this module to
  // function. These map to OveruseFrameDetector methods.
  // TODO(hbos): Define virtual methods in ResourceAdaptationModuleInterface
  // for input that are more generic so that this class can be used without
  // assumptions about underlying implementation.
  void StartCheckForOveruse(const CpuOveruseOptions& options);
  void StopCheckForOveruse();
  void FrameCaptured(const VideoFrame& frame, int64_t time_when_first_seen_us);
  void FrameSent(uint32_t timestamp,
                 int64_t time_sent_in_us,
                 int64_t capture_time_us,
                 absl::optional<int> encode_duration_us);
  void SetLastFramePixelCount(absl::optional<int> last_frame_pixel_count) {
    last_frame_pixel_count_ = last_frame_pixel_count;
  }

  void SetEncoder(VideoEncoder* encoder) { encoder_ = encoder; }
  void SetEncoderConfig(VideoEncoderConfig encoder_config) {
    encoder_config_ = std::move(encoder_config);
  }
  void SetCodecMaxFramerate(int codec_max_framerate) {
    codec_max_framerate_ = codec_max_framerate;
  }
  void SetEncoderStartBitrateBps(uint32_t encoder_start_bitrate_bps) {
    encoder_start_bitrate_bps_ = encoder_start_bitrate_bps;
  }
  // Inform the detector whether or not the quality scaler is enabled. This
  // helps GetActiveCounts() return absl::nullopt when appropriate.
  // TODO(hbos): This feels really hacky, can we report the right values without
  // this boolean? It would be really easy to report the wrong thing if this
  // method is called incorrectly.
  void SetIsQualityScalerEnabled(bool is_quality_scaler_enabled) {
    is_quality_scaler_enabled_ = is_quality_scaler_enabled;
  }

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference);
  void SetSourceWantsRotationApplied(bool rotation_applied);
  void SetSourceMaxFramerate(int max_framerate);
  void SetSourceMaxPixels(int max_pixels);

  // This is called on ReconfigureEncoder().
  // TODO(hbos): Can we get rid of it, and update accordingly whenever any of
  // the max framerates are updated instead?
  void RefreshTargetFramerate();
  void ResetAdaptationCounters();

 protected:
  friend class VideoStreamEncoder;

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

  // AdaptationObserverInterface implementation. Used both "internally" as
  // feedback from |overuse_detector_|, and externally from VideoStreamEncoder
  // when adaptation happens due to "DropDueToSize" at MaybeEncodeVideoFrame().
  // TODO(hbos): VideoStreamEncoder should not be responsible for any part of
  // the adaptation. Move this logic to this module?
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

  // Used by VideoStreamEncoder when ConfigureQualityScaler() occurs and the
  // |encoder_stats_observer_| is called outside of this class.
  // TODO(hbos): Decouple quality scaling and resource adaptation logic and make
  // this method private.
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptReason reason);

  // Used by VideoStreamEncoder::MaybeEncodeVideoFrame().
  // TODO(hbos): VideoStreamEncoder should not be responsible for any part of
  // the adaptation. Move this logic to this module?
  const AdaptCounter& GetConstAdaptCounter();

  // Used by VideoStreamEncoder::ConfigureQualityScaler().
  // TODO(hbos): Decouple quality scaling and resource adaptation logic and
  // delete this method.
  absl::optional<VideoEncoder::QpThresholds> GetQpThresholds() const;

 private:
  // TODO(hbos): RTC_GUARDED_BY(&encoder_queue_); everywhere...
  class VideoSourceProxy;

  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;
  };

  void UpdateAdaptationStats(AdaptReason reason);
  DegradationPreference EffectiveDegradataionPreference();
  AdaptCounter& GetAdaptCounter();
  bool CanAdaptUpResolution(int pixels, uint32_t bitrate_bps) const;

  rtc::TaskQueue* encoder_queue_;
  DegradationPreference degradation_preference_;
  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const DegradationPreference, AdaptCounter> adapt_counters_;
  const BalancedDegradationSettings balanced_settings_;
  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  absl::optional<AdaptationRequest> last_adaptation_request_;
  absl::optional<int> last_frame_pixel_count_;

  const std::unique_ptr<VideoSourceProxy> source_proxy_;
  const std::unique_ptr<OveruseFrameDetector> overuse_detector_;
  int codec_max_framerate_;
  uint32_t encoder_start_bitrate_bps_;
  bool is_quality_scaler_enabled_;
  VideoEncoderConfig encoder_config_;
  // TODO(hbos): Don't reconfigure the encoder directly. Instead, define the
  // output of a resource adaptation module as a struct and let the
  // VideoStreamEncoder handle the interaction with the actual encoder.
  VideoEncoder* encoder_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
