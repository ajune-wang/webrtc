/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/resource_adaptation_processor.h"

#include <limits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace adaptation {

const std::vector<std::unique_ptr<Resource>>&
ResourceAdaptationProcessor::resources() const {
  return resources_;
}

void ResourceAdaptationProcessor::AddResource(
    std::unique_ptr<Resource> resource) {
  resources_.push_back(std::move(resource));
}

const std::vector<std::unique_ptr<ResourceConsumerConfiguration>>&
ResourceAdaptationProcessor::configurations() const {
  return configurations_;
}

void ResourceAdaptationProcessor::AddConfiguration(
    std::unique_ptr<ResourceConsumerConfiguration> configuration) {
  configurations_.push_back(std::move(configuration));
}

const std::vector<ResourceConsumer>& ResourceAdaptationProcessor::consumers()
    const {
  return consumers_;
}

void ResourceAdaptationProcessor::AddConsumer(ResourceConsumer consumer) {
  consumers_.push_back(std::move(consumer));
}

std::pair<ResourceConsumer*, ResourceConsumerConfiguration*>
ResourceAdaptationProcessor::MitigateResourceUsageChange(
    const Resource& resource) {
  ResourceUsageState current_usage = resource.CurrentUsageState();
  if (current_usage == ResourceUsageState::kStable)
    return std::make_pair(nullptr, nullptr);
  if (current_usage == ResourceUsageState::kUnderuse) {
    // Underuse can only be mitigated if all resources are underused.
    for (const auto& resource : resources_) {
      if (resource->CurrentUsageState() != ResourceUsageState::kUnderuse)
        return std::make_pair(nullptr, nullptr);
    }
  }
  return FindOptimalConfiguration(resource, current_usage);
}

std::pair<ResourceConsumer*, ResourceConsumerConfiguration*>
ResourceAdaptationProcessor::FindOptimalConfiguration(
    const Resource& resource,
    ResourceUsageState current_usage) {
  RTC_DCHECK(current_usage == ResourceUsageState::kOveruse ||
             current_usage == ResourceUsageState::kUnderuse);
  std::pair<ResourceConsumer*, ResourceConsumerConfiguration*>
      best_configuration = std::make_pair(nullptr, nullptr);
  double best_configuration_delta_score =
      (current_usage == ResourceUsageState::kOveruse)
          ? 0.0
          : std::numeric_limits<double>::infinity();
  for (auto& consumer : consumers_) {
    double impact_before =
        consumer.configuration()->ApproximateImpact(resource);
    for (auto& neighbor_configuration : consumer.configuration()->neighbors()) {
      double impact_after = neighbor_configuration->ApproximateImpact(resource);
      double delta_score =
          (impact_after - impact_before) * consumer.degradation_preference();
      // Overuse: Find the highest magnitude negative delta.
      //   I.e. we want to go down as greatly as possible.
      // Underuse: Find the lowest magnitude positive delta.
      //   I.e. we want to go up as little as possible.
      if ((current_usage == ResourceUsageState::kOveruse &&
           delta_score < best_configuration_delta_score) ||
          (current_usage == ResourceUsageState::kUnderuse &&
           delta_score > 0.0 && delta_score < best_configuration_delta_score)) {
        best_configuration = std::make_pair(&consumer, neighbor_configuration);
        best_configuration_delta_score = delta_score;
      }
    }
  }
  return best_configuration;
}

}  // namespace adaptation
}  // namespace webrtc
