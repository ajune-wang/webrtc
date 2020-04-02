/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_H_
#define CALL_ADAPTATION_RESOURCE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

class Resource;
class VideoStreamInputState;  // TODO(hbos): This should move to call/.

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // Increasing the load on this resource is desirable, if possible.
  kUnderuse,
};

class ResourceListener {
 public:
  virtual ~ResourceListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource.usage_state()| is now up-to-date.
  //
  // The listener may influence the resource that signaled the measurement
  // according to the returned ResourceListenerResponse enum.
  virtual void OnResourceUsageStateMeasured(const Resource& resource) = 0;
};

// A Resource is something which can be measured as "overused", "stable" or
// "underused". When the resource usage changes, listeners of the resource are
// informed.
//
// Implementations of this interface are responsible for performing resource
// usage measurements and invoking OnResourceUsageStateMeasured().
class Resource {
 public:
  // By default, usage_state() is kStable until a measurement is made.
  Resource();
  virtual ~Resource();

  void RegisterListener(ResourceListener* listener);
  void UnregisterListener(ResourceListener* listener);

  virtual std::string name() const = 0;

  absl::optional<ResourceUsageState> usage_state() const;
  void ClearUsageState();
  virtual bool IsAdaptationAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      const Resource* reason_resource) const;
  virtual void DidApplyAdaptation(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after,
      const Resource* reason_resource);

 protected:
  // Updates the usage state and informs all registered listeners.
  void OnResourceUsageStateMeasured(
      absl::optional<ResourceUsageState> usage_state);

 private:
  absl::optional<ResourceUsageState> usage_state_;
  std::vector<ResourceListener*> listeners_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
