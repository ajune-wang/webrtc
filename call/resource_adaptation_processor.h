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
#include <utility>
#include <vector>

#include "call/resource.h"
#include "call/resource_consumer.h"
#include "call/resource_consumer_configuration.h"

namespace webrtc {
namespace adaptation {

struct NextConfiguration {
  NextConfiguration(ResourceConsumer* consumer,
                    ResourceConsumerConfiguration* configuration);

  ResourceConsumer* consumer;
  ResourceConsumerConfiguration* configuration;
};

class ResourceAdaptationProcessor {
 public:
  const std::vector<std::unique_ptr<Resource>>& resources() const;
  const std::vector<std::unique_ptr<ResourceConsumerConfiguration>>&
  configurations() const;
  const std::vector<std::unique_ptr<ResourceConsumer>>& consumers() const;

  // T = any subclass of Resource
  template <typename T>
  T* AddResource(std::unique_ptr<T> resource) {
    T* resource_ptr = resource.get();
    resources_.push_back(std::move(resource));
    return resource_ptr;
  }
  // T = any subclass of ResourceConsumerConfiguration
  template <typename T>
  T* AddConfiguration(std::unique_ptr<T> configuration) {
    T* configuration_ptr = configuration.get();
    configurations_.push_back(std::move(configuration));
    return configuration_ptr;
  }
  ResourceConsumer* AddConsumer(std::unique_ptr<ResourceConsumer> consumer);

  NextConfiguration FindNextConfiguration();

 private:
  ResourceConsumer* FindMostExpensiveConsumerThatCanBeAdaptedDown();
  ResourceConsumer* FindLeastExpensiveConsumerThatCanBeAdaptedUp();

  std::vector<std::unique_ptr<Resource>> resources_;
  std::vector<std::unique_ptr<ResourceConsumerConfiguration>> configurations_;
  std::vector<std::unique_ptr<ResourceConsumer>> consumers_;
};

}  // namespace adaptation
}  // namespace webrtc

#endif  // CALL_RESOURCE_ADAPTATION_PROCESSOR_H_
