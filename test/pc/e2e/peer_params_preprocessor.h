/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PC_E2E_PEER_PARAMS_PREPROCESSOR_H_
#define TEST_PC_E2E_PEER_PARAMS_PREPROCESSOR_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/async_resolver_factory.h"
#include "api/audio/audio_mixer.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/create_peer_connection_quality_test_frame_generator.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peer_network_dependencies.h"
#include "api/transport/network_control.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class PeerParamsPreprocessor {
 public:
  PeerParamsPreprocessor();
  ~PeerParamsPreprocessor();

  // Set missing params to default values if it is required:
  //  * Generate video stream labels if some of them are missing
  //  * Generate audio stream labels if some of them are missing
  //  * Set video source generation mode if it is not specified
  //  * Video codecs under test
  void SetDefaultValuesForMissingParams(PeerConfigurer& peer);

  // Validate peer's parameters, also ensure uniqueness of all video stream
  // labels.
  void ValidateParams(const PeerConfigurer& peer);

 private:
  class DefaultNamesProvider;
  std::unique_ptr<DefaultNamesProvider> peer_names_provider_;

  std::set<std::string> peer_names_;
  std::set<std::string> video_labels_;
  std::set<std::string> audio_labels_;
  std::set<std::string> video_sync_groups_;
  std::set<std::string> audio_sync_groups_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_PEER_PARAMS_PREPROCESSOR_H_
