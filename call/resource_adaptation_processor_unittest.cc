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

#include "call/resource.h"
#include "call/test/fake_resource.h"
#include "call/test/fake_resource_consumer_configuration.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace adaptation {

std::vector<FakeResourceConsumerConfiguration*>
AddStandardResolutionConfigurations(ResourceAdaptationProcessor* processor) {
  std::vector<std::unique_ptr<FakeResourceConsumerConfiguration>> configs;
  configs.push_back(std::make_unique<FakeResourceConsumerConfiguration>(
      1920, 1080, 30.0, 1.0));
  configs.push_back(std::make_unique<FakeResourceConsumerConfiguration>(
      1280, 720, 30.0, 1.0));
  configs.push_back(
      std::make_unique<FakeResourceConsumerConfiguration>(640, 360, 30.0, 1.0));
  configs.push_back(
      std::make_unique<FakeResourceConsumerConfiguration>(320, 180, 30.0, 1.0));
  for (size_t i = 1; i < configs.size(); ++i) {
    configs[i - 1]->AddLowerNeighbor(configs[i].get());
    configs[i]->AddUpperNeighbor(configs[i - 1].get());
  }
  std::vector<FakeResourceConsumerConfiguration*> config_ptrs;
  for (auto& config : configs) {
    config_ptrs.push_back(processor->AddConfiguration(std::move(config)));
  }
  return config_ptrs;
}

TEST(ResourceAdaptationProcessorTest,
     SingleStreamAndResourceDontAdaptDownWhenStable) {
  ResourceAdaptationProcessor processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kStable));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "UnnamedStream", resolution_configs[0]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_EQ(nullptr, next_config.consumer);
  EXPECT_EQ(nullptr, next_config.configuration);
}

TEST(ResourceAdaptationProcessorTest,
     SingleStreamAndResourceAdaptDownOnOveruse) {
  ResourceAdaptationProcessor processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* consumer = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "UnnamedStream", resolution_configs[0]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_EQ(consumer, next_config.consumer);
  EXPECT_EQ(resolution_configs[1], next_config.configuration);
}

TEST(ResourceAdaptationProcessorTest,
     SingleStreamAndResourceDontAdaptOnOveruseIfMinResolution) {
  ResourceAdaptationProcessor processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kOveruse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "UnnamedStream", resolution_configs.back()));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_EQ(nullptr, next_config.consumer);
  EXPECT_EQ(nullptr, next_config.configuration);
}

TEST(ResourceAdaptationProcessorTest,
     SingleStreamAndResourceAdaptUpOnUnderuse) {
  ResourceAdaptationProcessor processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  auto* consumer = processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "UnnamedStream", resolution_configs[1]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_EQ(consumer, next_config.consumer);
  EXPECT_EQ(resolution_configs[0], next_config.configuration);
}

TEST(ResourceAdaptationProcessorTest,
     SingleStreamAndResourceDontAdaptOnUnderuseIfMaxResolution) {
  ResourceAdaptationProcessor processor;
  processor.AddResource(
      std::make_unique<FakeResource>(ResourceUsageState::kUnderuse));
  auto resolution_configs = AddStandardResolutionConfigurations(&processor);
  processor.AddConsumer(std::make_unique<ResourceConsumer>(
      "UnnamedStream", resolution_configs[0]));
  auto next_config = processor.FindNextConfiguration();
  EXPECT_EQ(nullptr, next_config.consumer);
  EXPECT_EQ(nullptr, next_config.configuration);
}

}  // namespace adaptation
}  // namespace webrtc
