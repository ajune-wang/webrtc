/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RESOURCE_H_
#define CALL_RESOURCE_H_

#include <string>

namespace webrtc {
namespace adaptation {

enum class ResourceUsageState {
  // Usage exceeded, action is REQUIRED to minimze the load on this resource.
  kOveruse,
  // If usage is stable, increasing the resource load IS NOT a valid choice.
  kStable,
  // This resource is underused, increasing resource load for this resource is
  // a valid choice.
  kUnderuse,
};

class Resource {
 public:
  virtual ~Resource();

  // Informational, not formally part of the decision-making process.
  virtual std::string Name() const = 0;
  virtual std::string UsageUnitsOfMeasurement() const = 0;
  virtual double CurrentUsage() const = 0;

  // TODO(hbos): Add a polling frequency, with an asynchronous update
  // measurement method, and a "number of measurements before re-evaluating
  // ResourceUsageState". Something like CPU we might want to poll every second,
  // but something like temperature (in the future) we may want to poll every
  // 10 seconds and get several measurements to average before we report back a
  // new ResourceUsageState. We may want to have a callback for the
  // ResourceAdaptationProcessor to listen to.
  virtual ResourceUsageState CurrentUsageState() const = 0;

  std::string ToString() const;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_H_
