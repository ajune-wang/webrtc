/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RESOURCE_ADAPTATION_PROCESSOR_H_
#define CALL_RESOURCE_ADAPTATION_PROCESSOR_H_

#include <memory>
#include <vector>

#include "call/resource.h"
#include "call/resource_consumer.h"
#include "call/resource_consumer_configuration.h"

namespace webrtc {
namespace adaptation {

class ResourceAdaptationProcessor {
 public:
  const std::vector<std::unique_ptr<Resource>>& resources() const;
  void AddResource(std::unique_ptr<Resource> resource);

  const std::vector<std::unique_ptr<ResourceConsumerConfiguration>>&
  configurations() const;
  void AddConfiguration(
      std::unique_ptr<ResourceConsumerConfiguration> configuration);

  const std::vector<ResourceConsumer>& consumers() const;
  void AddConsumer(ResourceConsumer consumer);

  std::pair<ResourceConsumer*, ResourceConsumerConfiguration*>
  MitigateResourceUsageChange(const Resource& resource);

 private:
  std::pair<ResourceConsumer*, ResourceConsumerConfiguration*>
  FindOptimalConfiguration(const Resource& resource,
                           ResourceUsageState current_usage);

  std::vector<std::unique_ptr<Resource>> resources_;
  std::vector<std::unique_ptr<ResourceConsumerConfiguration>> configurations_;
  std::vector<ResourceConsumer> consumers_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_ADAPTATION_PROCESSOR_H_
