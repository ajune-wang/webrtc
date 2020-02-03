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

  virtual void OnResourceUsageStateMeasured(const Resource& resource,
                                            ResourceUsageState usage_state) = 0;
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
  void OnResourceUsageStateMeasured(ResourceUsageState usage_state);

 private:
  ResourceUsageState usage_state_;
  std::vector<ResourceUsageListener*> listeners_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_H_
