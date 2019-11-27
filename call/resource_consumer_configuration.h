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
#include <vector>

namespace webrtc {
namespace adaptation {

class Resource;

// Abstract class.
class ResourceConsumerConfiguration {
 public:
  explicit ResourceConsumerConfiguration(std::string name);
  virtual ~ResourceConsumerConfiguration();

  std::string name() const;
  const std::vector<ResourceConsumerConfiguration*>& neighbors() const;
  void AddNeighbor(ResourceConsumerConfiguration* neighbor);

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
  double ResourceImpactFactor(const Resource& resource) const;
  void SetResourceImpactFactor(const Resource* resource, double factor);

  // The approximate impact, in "weight", that this configuration has on the
  // resource. It is ApproximateCost() * ResourceImpactFactor(resource).
  double ApproximateImpact(const Resource& resource) const;

 private:
  std::string name_;
  std::map<const Resource*, double> impact_factor_by_resource_;
  // Configurations we can adapt to when we are in |this| configuration.
  std::vector<ResourceConsumerConfiguration*> neighbors_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_CONSUMER_CONFIGURATION_H_
