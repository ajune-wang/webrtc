/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_VOIP_VOIP_CORE_H_
#define AUDIO_VOIP_VOIP_CORE_H_

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/voip/voip_base.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_engine.h"
#include "api/voip/voip_network.h"
#include "audio/audio_transport_impl.h"
#include "audio/voip/audio_channel.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/critical_section.h"

namespace webrtc {

// VoipCore is the implementatino of VoIP APIs listed in api/voip directory.
// It manages a vector of AudioChannel objects where each is mapped with a
// ChannelId (int) type. ChannelId is the primary key to locate a specific
// AudioChannel object to operate requested VoIP API from the caller.
//
// This class receives required audio components from caller at construction and
// owns the life cycle of them to orchestrate the proper destruction sequences.
class VoipCore : public VoipEngine,
                 public VoipBase,
                 public VoipNetwork,
                 public VoipCodec {
 public:
  ~VoipCore() override = default;

  // Initialize VoipCore components with provided arguments.
  // Returns false only when |audio_device| fails to initialize which would
  // render further processing useless. Note that failures on initializing
  // default recording/speaker devices are not considered to be fatal here but
  // will manifest when caller tries to StartSend/StartPlayout later. In certain
  // case, caller may not care about recording device functioning (e.g webinar
  // where only speaker is available). It's also possible that there are other
  // audio devices available that may work.
  bool Init(rtc::scoped_refptr<AudioEncoderFactory> encoder_factory,
            rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
            std::unique_ptr<TaskQueueFactory> task_queue_factory,
            rtc::scoped_refptr<AudioDeviceModule> audio_device,
            rtc::scoped_refptr<AudioProcessing> audio_processing);

  // Implements VoipEngine interfaces.
  VoipBase& Base() override;
  VoipNetwork& Network() override;
  VoipCodec& Codec() override;

  // Implements VoipBase interfaces.
  absl::optional<ChannelId> CreateChannel(
      Transport* transport,
      absl::optional<uint32_t> local_ssrc) override;
  void ReleaseChannel(ChannelId channel) override;
  bool StartSend(ChannelId channel) override;
  bool StopSend(ChannelId channel) override;
  bool StartPlayout(ChannelId channel) override;
  bool StopPlayout(ChannelId channel) override;

  // Implements VoipNetwork interfaces.
  void ReceivedRTPPacket(ChannelId channel,
                         rtc::ArrayView<const uint8_t> data) override;
  void ReceivedRTCPPacket(ChannelId channel,
                          rtc::ArrayView<const uint8_t> data) override;

  // Implements VoipCodec interfaces.
  void SetSendCodec(ChannelId channel,
                    int payload_type,
                    const SdpAudioFormat& encoder_format) override;
  void SetReceiveCodecs(
      ChannelId channel,
      const std::map<int, SdpAudioFormat>& decoder_specs) override;

 private:
  // Internal method to fetch scoped pointer of AudioChannel using |channel|.
  rtc::scoped_refptr<AudioChannel> GetChannel(ChannelId channel);

  // Internal method to synchronize the set of AudioSender (AudioEgress) with
  // AudioTransportImpl.
  bool UpdateAudioTransportWithSenders();

  // Listed in order for safe destruction of voip core object. These members
  // are used to configure AudioChannel during its construction.
  // Synchronization for these are handled internally.
  std::unique_ptr<AudioTransportImpl> audio_transport_;
  rtc::scoped_refptr<AudioProcessing> audio_processing_;
  rtc::scoped_refptr<AudioMixer> audio_mixer_;
  rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  rtc::scoped_refptr<AudioDeviceModule> audio_device_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<ProcessThread> process_thread_;

  rtc::CriticalSection lock_;

  // AudioChannel is assigned with ChannelId which coincides with the index
  // ordering of the container so that each AudioChannel is fetched directly
  // using ChannelId as container index. Once AudioChannel is released, the
  // element will be set to nullptr to indicate as invalidated one.
  std::vector<rtc::scoped_refptr<AudioChannel>> channels_ RTC_GUARDED_BY(lock_);

  // ChannelId queue to track the list of released ChannelId that are reused
  // on AudioChannel creation into vector above.
  std::queue<ChannelId> idle_id_ RTC_GUARDED_BY(lock_);
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_VOIP_CORE_H_
