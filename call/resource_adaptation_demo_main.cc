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

#include "absl/types/optional.h"
#include "call/resource_adaptation_processor.h"
#include "call/resource_consumer_configuration.h"
#include "call/resource_consumer.h"
#include "call/resource.h"
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

const char* ResolutionName(Resolution resolution) {
  switch (resolution) {
    case Resolution::k180p:
      return "QVGA (180p)";
    case Resolution::k360p:
      return "VGA (360p)";
    case Resolution::k720p:
      return "HD (720p)";
    case Resolution::k1080p:
      return "Full HD (1080p)";
    case Resolution::k1440p:
      return "QHD (1440p)";
    case Resolution::k2160p:
      return "4K (2160p)";
  }
}

absl::optional<Resolution> NextResolutionUp(Resolution resolution) {
  switch (resolution) {
    case Resolution::k180p:
      return Resolution::k360p;
    case Resolution::k360p:
      return Resolution::k720p;
    case Resolution::k720p:
      return Resolution::k1080p;
    case Resolution::k1080p:
      return Resolution::k1440p;
    case Resolution::k1440p:
      return Resolution::k2160p;
    case Resolution::k2160p:
      return absl::nullopt;
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

class ResolutionResourceConsumerConfiguration
    : public ResourceConsumerConfiguration {
 public:
  ResolutionResourceConsumerConfiguration(Resolution resolution,
                                          double frame_rate)
      : ResourceConsumerConfiguration(
            std::string(ResolutionName(resolution)) + " @ " +
            rtc::ToString(frame_rate)),
        frame_rate_(frame_rate) {
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
    return width_ * height_ * frame_rate_;
  }

 private:
  int width_;
  int height_;
  double frame_rate_;
};

class Demo {
 public:
  Demo() {
    processor_.AddResource(std::make_unique<FakeCpuResource>(0.75));
  }

  // Constructs a configuration graph for the stream, returning the highest
  // resolution configuration.
  void AddStream(std::string name, double degradation_preference,
                 Resolution max_resolution, double frame_rate) {
    // Configs, in descending resolution order.
    std::vector<std::unique_ptr<ResolutionResourceConsumerConfiguration>>
        configs;
    for (absl::optional<Resolution> resolution = max_resolution;
         resolution.has_value();
         resolution = NextResolutionDown(resolution.value())) {
      configs.push_back(
          std::make_unique<ResolutionResourceConsumerConfiguration>(
              resolution.value(), frame_rate));
    }
    for (size_t i = 1; i < configs.size(); ++i) {
      configs[i - 1]->AddNeighbor(configs[i].get());
      configs[i]->AddNeighbor(configs[i - 1].get());
    }
    auto* highest_resolution_config = configs[0].get();
    for (auto& config : configs) {
      processor_.AddConfiguration(std::move(config));
    }
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

 private:
  ResourceAdaptationProcessor processor_;
};

}  // namespace adaptation
}  // namespace webrtc

int main(int argc, char* argv[]) {
  webrtc::adaptation::Demo demo;
  demo.AddStream(
      "Camera 720p@30fps", 1.0, webrtc::adaptation::Resolution::k720p, 30.0);
  demo.AddStream("Screenshare 1080p@15fps", 1.0,
                 webrtc::adaptation::Resolution::k1080p, 15.0);
  printf("%s\n", demo.CurrentStateString().c_str());
}
