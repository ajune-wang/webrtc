/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
#define VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/resource.h"
#include "modules/video_coding/utility/quality_scaler.h"

namespace webrtc {

class ResourceAdaptationProcessor;

enum class ResourceListenerResponse {
  kNothing,
  // This response is only applicable to QualityScaler-based resources.
  // It tells the QualityScaler to increase its QP measurement frequency.
  //
  // This is modelled after AdaptationObserverInterface::AdaptDown()'s return
  // value. The method comment says "Returns false if a downgrade was requested
  // but the request did not result in a new limiting resolution or fps."
  // However the actual implementation seems to be: Return false if
  // !has_input_video_ or if we use balanced degradation preference and we DID
  // adapt frame rate but the difference between input frame rate and balanced
  // settings' min fps is less than the balanced settings' min fps diff - in all
  // other cases, return true whether or not adaptation happened.
  //
  // For QualityScaler-based resources, kQualityScalerShouldIncreaseFrequency
  // maps to "return false" and kNothing maps to "return true".
  //
  // TODO(https://crbug.com/webrtc/11222): Remove this enum. Resource
  // measurements and adaptation decisions need to be separated in order to
  // support injectable adaptation modules, multi-stream aware adaptation and
  // decision-making logic based on multiple resources.
  kQualityScalerShouldIncreaseFrequency,
};

// Handles interaction with the QualityScaler.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly by usage in the ResourceAdaptationProcessor (which is only tested
// because of its usage in VideoStreamEncoder); all tests are currently in
// video_stream_encoder_unittest.cc.
// TODO(https://crbug.com/webrtc/11222): Move this class to the
// video/adaptation/ subdirectory.
class QualityScalerResource : public Resource,
                              public AdaptationObserverInterface {
 public:
  explicit QualityScalerResource(
      ResourceAdaptationProcessor* adaptation_processor);

  bool is_started() const;

  void StartCheckForOveruse(VideoEncoder::QpThresholds qp_thresholds);
  void StopCheckForOveruse();

  void SetQpThresholds(VideoEncoder::QpThresholds qp_thresholds);
  bool QpFastFilterLow();
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us);
  void OnFrameDropped(EncodedImageCallback::DropReason reason);

  // AdaptationObserverInterface implementation.
  // TODO(https://crbug.com/webrtc/11222, 11172): This resource also needs to
  // signal when its stable to support multi-stream aware modules.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

  std::string name() const override { return "QualityScalerResource"; }

  void DidApplyAdaptation(const VideoStreamInputState& input_state,
                          const VideoSourceRestrictions& restrictions_before,
                          const VideoSourceRestrictions& restrictions_after,
                          const Resource* reason_resource) override;
  absl::optional<ResourceListenerResponse> last_adaptation_applied_response()
      const {
    return last_adaptation_applied_response_;
  }

 private:
  std::unique_ptr<QualityScaler> quality_scaler_;
  ResourceAdaptationProcessor* adaptation_processor_;
  absl::optional<ResourceListenerResponse> last_adaptation_applied_response_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_QUALITY_SCALER_RESOURCE_H_
