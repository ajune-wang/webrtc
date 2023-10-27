/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_time_controller.h"

#include <memory>
#include <utility>

#include "api/media_factory/create_media_factory.h"
#include "call/call.h"
#include "call/rtp_transport_config.h"
#include "call/rtp_transport_controller_send_factory_interface.h"
#include "media/engine/webrtc_media_engine.h"
#include "test/time_controller/external_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

class MediaFactoryForTest : public MediaFactory {
 public:
  explicit MediaFactoryForTest(MediaFactoryForTestParams params)
      : test_params_(std::move(params)) {}

 private:
  std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
    if (test_params_.time_controller == nullptr) {
      return prod_factory_->CreateCall(config);
    }
    Clock* clock = test_params_.time_controller->GetClock();
    return Call::Create(config, clock,
                        config.rtp_transport_controller_send_factory->Create(
                            config.ExtractTransportConfig(), clock));
  }

  std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& deps) override {
    RTC_CHECK(!created_media_engine_)
        << "CreateMediaEngine should be called at most once";
    created_media_engine_ = true;
    if (test_params_.media_engine == nullptr) {
      return prod_factory_->CreateMediaEngine(deps);
    }
    return std::move(test_params_.media_engine);
  }

  bool created_media_engine_ = false;
  const std::unique_ptr<MediaFactory> prod_factory_ = CreateMediaFactory();
  MediaFactoryForTestParams test_params_;
};

std::unique_ptr<TimeController> CreateTimeController(
    ControlledAlarmClock* alarm) {
  return std::make_unique<ExternalTimeController>(alarm);
}

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  return std::make_unique<GlobalSimulatedTimeController>(
      Timestamp::Seconds(10000));
}

std::unique_ptr<CallFactoryInterface> CreateTimeControllerBasedCallFactory(
    TimeController* time_controller) {
  class TimeControllerBasedCallFactory : public CallFactoryInterface {
   public:
    explicit TimeControllerBasedCallFactory(TimeController* time_controller)
        : time_controller_(time_controller) {}
    std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
      RtpTransportConfig transportConfig = config.ExtractTransportConfig();

      return Call::Create(config, time_controller_->GetClock(),
                          config.rtp_transport_controller_send_factory->Create(
                              transportConfig, time_controller_->GetClock()));
    }

   private:
    TimeController* time_controller_;
  };
  return std::make_unique<TimeControllerBasedCallFactory>(time_controller);
}

std::unique_ptr<MediaFactory> CreateMediaFactoryForTest(
    MediaFactoryForTestParams params) {
  return std::make_unique<MediaFactoryForTest>(std::move(params));
}

}  // namespace webrtc
