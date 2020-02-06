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

#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

class Resource;

enum class ResourceUsageState {
  // Action is needed to minimze the load on this resource.
  kOveruse,
  // No action needed for this resource, increasing the load on this resource
  // is not allowed.
  kStable,
  // Increasing the load on this resource is allowed.
  kUnderuse,
};

class ResourceUsageListener {
 public:
  virtual ~ResourceUsageListener();

  // Informs the listener of a new measurement of resource usage. This means
  // that |resource.usage_state()| is now up-to-date.
  //
  // The listener may optionally return "whether or not adaptation happened as a
  // result". Note that this is mirroring AdaptationObserverInterface's
  // AdaptDown() method and current implementations may behave differently than
  // this contract, see 2) below.
  // TODO(https://crbug.com/webrtc/11222): Remove this return value or
  // investigate alternative means to achieve what the implementation is
  // currently doing. Reasons to abandon current approach:
  // 1) Resource usage measurements and adaptation decisions need to be
  //    separated in order to support injectable adaptation modules,
  //    multi-stream aware adaptation and decision-making logic based on
  //    multiple resources. If the resource depends on the the latest adaptation
  //    perhaps it should listen to VideoSourceRestriction updates instead?
  // 2) The implementation currently does not seem to do what the interfaces
  //    says. Documentation for AdaptDown(): "Returns false if a downgrade was
  //    requested but the request did not result in a new limiting resolution or
  //    fps." The implementation: Return false if !has_input_video_ or if we use
  //    balanced degradation preference and we DID adapt frame rate but the
  //    difference between input frame rate and balanced settings' min fps is
  //    less than the balanced settings' min fps diff - in all other cases,
  //    return true whether or not adaptation happened.
  // The true purpose of this return value seem to be that if its false the
  // QualityScaler may check quality more often but by default we return true.
  // This kind of logic should move out of
  // overuse_frame_detector_resource_adaptation_module.cc.
  virtual absl::optional<bool> OnResourceUsageStateMeasured(
      const Resource& resource) = 0;
};

// A Resource is something which can be measured as "overused", "stable" or
// "underused". When the resource usage changes, listeners of the resource are
// informed.
//
// Implementations of this interface are responsible for performing resource
// usage measurements and invoking OnResourceUsageStateMeasured().
class Resource {
 public:
  Resource();
  virtual ~Resource();

  void RegisterListener(ResourceUsageListener* listener);

  ResourceUsageState usage_state() const;

 protected:
  // Updates the usage state and informs all registered listeners.
  // Returns false if any listeners' OnResourceUsageStateMeasured() returned
  // false.
  bool OnResourceUsageStateMeasured(ResourceUsageState usage_state);

 private:
  ResourceUsageState usage_state_;
  std::vector<ResourceUsageListener*> listeners_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
