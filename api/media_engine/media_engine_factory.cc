/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_engine/media_engine_factory.h"

#include <utility>

#include "api/peer_connection_interface.h"
#include "call/call_factory.h"
#include "media/engine/webrtc_media_engine.h"

namespace webrtc {
namespace {

using ::cricket::MediaEngineDependencies;
using ::cricket::MediaEngineInterface;

class MediaEngineFactory : public MediaEngineFactoryInterface {
 public:
  MediaEngineFactory() = default;
  MediaEngineFactory(const MediaEngineFactory&) = delete;
  MediaEngineFactory& operator=(const MediaEngineFactory&) = delete;
  ~MediaEngineFactory() override = default;

 private:
  std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
    return CallFactory().CreateCall(config);
  }

  std::unique_ptr<MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& deps) override {
    cricket::MediaEngineDependencies media_deps;
    // Common.
    media_deps.task_queue_factory = deps.task_queue_factory.get();
    media_deps.trials = deps.trials.get();
    // Audio.
    media_deps.adm = std::move(deps.adm);
    media_deps.audio_encoder_factory = std::move(deps.audio_encoder_factory);
    media_deps.audio_decoder_factory = std::move(deps.audio_decoder_factory);
    media_deps.audio_mixer = std::move(deps.audio_mixer);
    media_deps.audio_processing = std::move(deps.audio_processing);
    media_deps.audio_frame_processor = deps.raw_audio_frame_processor;
    media_deps.owned_audio_frame_processor =
        std::move(deps.audio_frame_processor);
    // Video
    media_deps.video_encoder_factory = std::move(deps.video_encoder_factory);
    media_deps.video_decoder_factory = std::move(deps.video_decoder_factory);
    return cricket::CreateMediaEngine(std::move(media_deps));
  }
};

}  // namespace

std::unique_ptr<MediaEngineFactoryInterface> CreateMediaEngineFactory() {
  return std::make_unique<MediaEngineFactory>();
}

}  // namespace webrtc
