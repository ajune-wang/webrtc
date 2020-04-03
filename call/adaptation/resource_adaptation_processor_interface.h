/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

// TODO(hbos): When AdaptationCounters is moved to the call/ folder (CL:
// https://webrtc-review.googlesource.com/c/src/+/171685), include it instead of
// forward-declaring it.
struct AdaptationCounters;

// The listener is responsible for carrying out the reconfiguration of the video
// source such that the VideoSourceRestrictions are fulfilled.
class ResourceAdaptationProcessorListener {
 public:
  virtual ~ResourceAdaptationProcessorListener();

  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const AdaptationCounters& adaptation_counters,
      const Resource* reason) = 0;
};

class ResourceAdaptationProcessorInterface {
 public:
  virtual ~ResourceAdaptationProcessorInterface();

  virtual DegradationPreference degradation_preference() const = 0;
  virtual DegradationPreference effective_degradation_preference() const = 0;

  virtual void StartResourceAdaptation() = 0;
  virtual void StopResourceAdaptation() = 0;
  // TODO(hbos): Adding and removing resources should implicitly start and stop?
  virtual void AddResource(Resource* resource) = 0;

  virtual void SetDegradationPreference(
      DegradationPreference degradation_preference) = 0;
  virtual void SetIsScreenshare(bool is_screenshare) = 0;
  virtual void ResetVideoSourceRestrictions() = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
