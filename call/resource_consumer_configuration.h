/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RESOURCE_CONSUMER_CONFIGURATION_H_
#define CALL_RESOURCE_CONSUMER_CONFIGURATION_H_

#include <map>
#include <string>

namespace webrtc {
namespace adaptation {

class Resource;

// Abstract class.
class ResourceConsumerConfiguration {
 public:
  ResourceConsumerConfiguration(std::string name);
  virtual ~ResourceConsumerConfiguration();

  std::string name() const;

  // How expensive this configuration is. The unit is "weight". This is an
  // abstract unit used by the ResourceAdaptationProcessor to compare
  // configurations. See also ResourceImpactFactor().
  //
  // For encoder consumer configurations, this value should scale with pixels
  // per second.
  virtual double ApproximateCost() const = 0;

  // The impact factor of this configuration on the resource. The unit is
  // "weight". This is an abstract unit used by the ResourceAdaptationProcessor
  // to evaluate the expensiveness of configurations.
  //
  // The ApproximateCost() multiplied by the ResourceImpactFactor(resource) is
  // the total impact, in "weight", that this configuration has on |resource|.
  //
  // By default, the impact factor is 1.0. For custom factors, use protected
  // method SetResourceImpactFactor().
  double ResourceImpactFactor(Resource* resource) const;

 protected:
  void SetResourceImpactFactor(Resource* resource, double factor);

 private:
  std::string name_;
  std::map<Resource*, double> impact_factor_by_resource_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_CONSUMER_CONFIGURATION_H_
