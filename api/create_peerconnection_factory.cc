/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/create_peerconnection_factory.h"

#include <memory>
#include <utility>

#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/call/call_factory_interface.h"
#include "api/fec_controller.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/global_task_queue_factory.h"
#include "api/transport/network_control.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory.h"
#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "media/base/media_engine.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/thread.h"

namespace webrtc {

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    rtc::scoped_refptr<AudioDeviceModule> default_adm,
    rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory,
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory,
    rtc::scoped_refptr<AudioMixer> audio_mixer,
    rtc::scoped_refptr<AudioProcessing> audio_processing) {
  cricket::MediaEngineDependencies media_dependencies;
  media_dependencies.task_queue_factory = &GlobalTaskQueueFactory();
  media_dependencies.adm = std::move(default_adm);
  media_dependencies.audio_encoder_factory = std::move(audio_encoder_factory);
  media_dependencies.audio_decoder_factory = std::move(audio_decoder_factory);
  media_dependencies.audio_mixer = std::move(audio_mixer);
  if (audio_processing)
    media_dependencies.audio_processing = std::move(audio_processing);
  else
    media_dependencies.audio_processing = AudioProcessingBuilder().Create();

  media_dependencies.video_encoder_factory = std::move(video_encoder_factory);
  media_dependencies.video_decoder_factory = std::move(video_decoder_factory);
  media_dependencies.video_bitrate_allocator_factory =
      CreateBuiltinVideoBitrateAllocatorFactory();

  PeerConnectionFactoryDependencies dependencies;
  dependencies.network_thread = network_thread;
  dependencies.worker_thread = worker_thread;
  dependencies.signaling_thread = signaling_thread;
  dependencies.media_engine =
      cricket::CreateMediaEngine(std::move(media_dependencies));
  dependencies.call_factory = CreateCallFactory();
  dependencies.event_log_factory = CreateRtcEventLogFactory();
  return CreateModularPeerConnectionFactory(std::move(dependencies));
}

}  // namespace webrtc
