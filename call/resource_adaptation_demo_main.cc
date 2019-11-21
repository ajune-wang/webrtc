/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <map>
#include <string>

#include "absl/types/optional.h"
#include "call/resource.h"
#include "call/resource_adaptation_processor.h"
#include "call/resource_consumer.h"
#include "call/resource_consumer_configuration.h"
#include "call/test/fake_resources.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace adaptation {

enum class Resolution {
  k180p,
  k360p,
  k720p,
  k1080p,
  k1440p,
  k2160p,
};

const char* ResolutionFullName(Resolution resolution) {
  switch (resolution) {
    case Resolution::k180p:
      return "QVGA";
    case Resolution::k360p:
      return "VGA";
    case Resolution::k720p:
      return "HD";
    case Resolution::k1080p:
      return "Full HD";
    case Resolution::k1440p:
      return "QHD";
    case Resolution::k2160p:
      return "4K";
  }
}

const char* ResolutionPixelName(Resolution resolution) {
  switch (resolution) {
    case Resolution::k180p:
      return "180p";
    case Resolution::k360p:
      return "360p";
    case Resolution::k720p:
      return "720p";
    case Resolution::k1080p:
      return "1080p";
    case Resolution::k1440p:
      return "1440p";
    case Resolution::k2160p:
      return "2160p";
  }
}

absl::optional<Resolution> NextResolutionDown(Resolution resolution) {
  switch (resolution) {
    case Resolution::k180p:
      return absl::nullopt;
    case Resolution::k360p:
      return Resolution::k180p;
    case Resolution::k720p:
      return Resolution::k360p;
    case Resolution::k1080p:
      return Resolution::k720p;
    case Resolution::k1440p:
      return Resolution::k1080p;
    case Resolution::k2160p:
      return Resolution::k1440p;
  }
}

enum class FrameRate {
  k30,
  k20,
  k15,
  k10,
  k5,
  k1,
};

double FrameRateToNumber(FrameRate frame_rate) {
  switch (frame_rate) {
    case FrameRate::k30:
      return 30.0;
    case FrameRate::k20:
      return 20.0;
    case FrameRate::k15:
      return 15.0;
    case FrameRate::k10:
      return 10.0;
    case FrameRate::k5:
      return 5.0;
    case FrameRate::k1:
      return 1.0;
  }
}

// When scaling the cost like this it's easy to flip the sign of the delta.
// Perhaps it would be less error-prone if we could...
// - Separate "upgrade" and "downgrade" neighbors into separate list and not
//   look at the sign, or,
// - Apply the penalty after the impact is calculated, such that we can only
//   increase or decrease in magnitude rather than tweak the totals.
// - Preference ratio multiplied by impact factor maybe?
double FrameRatePenaltyFactor(FrameRate frame_rate) {
  return 1.0;
  // switch (frame_rate) {
  //   case FrameRate::k30:
  //     return 8.0;
  //   case FrameRate::k20:
  //     return 2.0;
  //   case FrameRate::k15:
  //     return 0.5;
  //   case FrameRate::k10:
  //     return 2.0;
  //   case FrameRate::k5:
  //     return 8.0;
  //   case FrameRate::k1:
  //     return 20.0;
  // }
}

absl::optional<FrameRate> NextFrameRateDown(FrameRate frame_rate) {
  switch (frame_rate) {
    case FrameRate::k30:
      return FrameRate::k20;
    case FrameRate::k20:
      return FrameRate::k15;
    case FrameRate::k15:
      return FrameRate::k10;
    case FrameRate::k10:
      return FrameRate::k5;
    case FrameRate::k5:
      return FrameRate::k1;
    case FrameRate::k1:
      return absl::nullopt;
  }
}

class ResolutionResourceConsumerConfiguration
    : public ResourceConsumerConfiguration {
 public:
  ResolutionResourceConsumerConfiguration(Resolution resolution,
                                          double frame_rate,
                                          double penalty_factor)
      : ResourceConsumerConfiguration(
            std::string(ResolutionPixelName(resolution)) + " @ " +
            rtc::ToString(frame_rate)),
        frame_rate_(frame_rate),
        penalty_factor_(penalty_factor) {
    switch (resolution) {
      case Resolution::k180p:
        width_ = 320;
        height_ = 180;
        break;
      case Resolution::k360p:
        width_ = 640;
        height_ = 360;
        break;
      case Resolution::k720p:
        width_ = 1280;
        height_ = 720;
        break;
      case Resolution::k1080p:
        width_ = 1920;
        height_ = 1080;
        break;
      case Resolution::k1440p:
        width_ = 2560;
        height_ = 1440;
        break;
      case Resolution::k2160p:
        width_ = 3840;
        height_ = 2160;
        break;
    }
  }

  int width() const { return width_; }
  int height() const { return height_; }
  double frame_rate() const { return frame_rate_; }

  double ApproximateCost() const override {
    return width_ * height_ * frame_rate_ * penalty_factor_;
  }

 private:
  int width_;
  int height_;
  double frame_rate_;
  double penalty_factor_;
};

class DisabledResourceConsumerConfiguration
    : public ResourceConsumerConfiguration {
 public:
  DisabledResourceConsumerConfiguration()
      : ResourceConsumerConfiguration("DISABLED") {}

  double ApproximateCost() const override { return 0.0; }
};

class Demo {
 public:
  Demo() {
    auto resource = std::make_unique<FakeCpuResource>(0.7);
    resource_ = resource.get();
    processor_.AddResource(std::move(resource));
  }

  // Constructs a configuration graph for the stream, returning the highest
  // resolution configuration.
  void AddStream(std::string name,
                 double degradation_preference,
                 Resolution max_resolution,
                 bool downgrade_resolution,
                 FrameRate max_frame_rate,
                 bool downgrade_frame_rate) {
    // Configuration matrix: frame rate by resolution.
    std::vector<std::vector<std::unique_ptr<ResourceConsumerConfiguration>>>
        configs;
    for (absl::optional<Resolution> resolution = max_resolution;
         resolution.has_value();
         resolution = downgrade_resolution
                          ? NextResolutionDown(resolution.value())
                          : absl::nullopt) {
      configs.emplace_back();
      for (absl::optional<FrameRate> frame_rate = max_frame_rate;
           frame_rate.has_value();
           frame_rate = downgrade_frame_rate
                            ? NextFrameRateDown(frame_rate.value())
                            : absl::nullopt) {
        configs.back().push_back(
            std::make_unique<ResolutionResourceConsumerConfiguration>(
                resolution.value(), FrameRateToNumber(frame_rate.value()),
                FrameRatePenaltyFactor(frame_rate.value())));
      }
    }
    auto* highest_resolution_config = configs[0][0].get();
    // Relationships: Like a grid, frame rate or resolution.
    for (size_t x = 0; x < configs.size(); ++x) {
      for (size_t y = 0; y < configs[x].size(); ++y) {
        if (x != 0) {
          configs[x - 1][y]->AddNeighbor(configs[x][y].get());
          configs[x][y]->AddNeighbor(configs[x - 1][y].get());
        }
        if (y != 0) {
          configs[x][y - 1]->AddNeighbor(configs[x][y].get());
          configs[x][y]->AddNeighbor(configs[x][y - 1].get());
        }
      }
    }
    // The disabled config: Reachable only from the lowest frame rate and
    // resolution.
    auto disabled_config =
        std::make_unique<DisabledResourceConsumerConfiguration>();
    configs.back().back()->AddNeighbor(disabled_config.get());
    disabled_config->AddNeighbor(configs.back().back().get());
    // Pass ownership of all configs.
    for (size_t x = 0; x < configs.size(); ++x) {
      for (size_t y = 0; y < configs[x].size(); ++y) {
        processor_.AddConfiguration(std::move(configs[x][y]));
      }
    }
    processor_.AddConfiguration(std::move(disabled_config));
    // Create a consumer for the stream.
    ResourceConsumer stream(std::move(name), highest_resolution_config,
                            degradation_preference);
    processor_.AddConsumer(std::move(stream));
  }

  std::string CurrentStateString() {
    rtc::StringBuilder sb;
    for (const auto& consumer_stream : processor_.consumers()) {
      sb << consumer_stream.name() << " [ApproximateCost: ";
      sb << rtc::ToString(consumer_stream.configuration()->ApproximateCost());
      sb << "]\n  " << consumer_stream.configuration()->name() << "\n";
    }
    for (const auto& resource : processor_.resources()) {
      sb << "\n" << resource->ToString();
    }
    return sb.str();
  }

  void SetResourceUsage(ResourceUsageState state) {
    switch (state) {
      case ResourceUsageState::kOveruse:
        resource_->set_usage(0.8);
        break;
      case ResourceUsageState::kStable:
        resource_->set_usage(0.7);
        break;
      case ResourceUsageState::kUnderuse:
        resource_->set_usage(0.6);
        break;
    }
  }

  void Debug() {
    for (auto const& config : processor_.configurations()) {
      printf("%s @ %f\n", config->name().c_str(), config->ApproximateCost());
      std::map<double, const ResourceConsumerConfiguration*>
          neighbors_by_delta_cost;
      for (auto const* neighbor : config->neighbors()) {
        neighbors_by_delta_cost.insert(std::make_pair(
            neighbor->ApproximateCost() - config->ApproximateCost(), neighbor));
      }
      for (auto const& pair : neighbors_by_delta_cost) {
        auto const* neighbor = pair.second;
        printf("- %s @ %f", neighbor->name().c_str(),
               neighbor->ApproximateCost());
        printf(" | DELTA: %f | RATIO: %f\n",
               neighbor->ApproximateCost() - config->ApproximateCost(),
               neighbor->ApproximateCost() / config->ApproximateCost());
      }
    }
  }

  void MitigateResourceUsageChange() {
    auto pair = processor_.MitigateResourceUsageChange(*resource_);
    if (pair.first) {
      pair.first->SetConfiguration(pair.second);
    }
  }

 private:
  ResourceAdaptationProcessor processor_;
  FakeCpuResource* resource_;
};

}  // namespace adaptation
}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::adaptation::Demo demo;
  demo.AddStream("Camera [Native: 720p @ 30fps]", 1.0,
                 webrtc::adaptation::Resolution::k720p, true,
                 webrtc::adaptation::FrameRate::k30, false);
  demo.AddStream("Screenshare [Native: 1080p @ 15fps]", 1.0,
                 webrtc::adaptation::Resolution::k1080p, false,
                 webrtc::adaptation::FrameRate::k15, true);
  // demo.Debug();
  while (true) {
    printf("%s\n", demo.CurrentStateString().c_str());
    while (true) {
      printf("> ");
      std::string in;
      std::getline(std::cin, in);
      if (in == "q")
        return 0;
      if (in == "") {
        demo.SetResourceUsage(webrtc::adaptation::ResourceUsageState::kStable);
        break;
      } else if (in == "+") {
        demo.SetResourceUsage(webrtc::adaptation::ResourceUsageState::kOveruse);
        break;
      } else if (in == "0") {
        demo.SetResourceUsage(webrtc::adaptation::ResourceUsageState::kStable);
        break;
      } else if (in == "-") {
        demo.SetResourceUsage(
            webrtc::adaptation::ResourceUsageState::kUnderuse);
        break;
      }
    }
    demo.MitigateResourceUsageChange();
  }
  return 0;
}
