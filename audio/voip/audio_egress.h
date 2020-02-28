//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef AUDIO_VOIP_AUDIO_EGRESS_H_
#define AUDIO_VOIP_AUDIO_EGRESS_H_

#include <memory>
#include <string>

#include "api/audio_codecs/audio_format.h"
#include "api/task_queue/task_queue_factory.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/audio_sender.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

class AudioEgress : public AudioSender, public AudioPacketizationCallback {
 public:
  AudioEgress(std::shared_ptr<RtpRtcp> rtp_rtcp,
              Clock* clock,
              TaskQueueFactory* task_queue_factory);
  ~AudioEgress() override;

  // Send using this encoder, with this payload type.
  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder);

  // These are virtual as expected to be controlled by AudioChannel.
  virtual void StartSend();
  virtual void StopSend();

  bool Sending() const;
  void Mute(bool mute);

  // Retrieve current encoder related info
  int EncoderSampleRate();
  size_t EncoderNumChannel();

  void RegisterTelephoneEventType(int payload_type, int payload_frequency);
  bool SendTelephoneEventOutband(int event, int duration_ms);

 private:
  void ProcessMuteState(AudioFrame* audio_frame);

  // AudioSender interface
  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;

  // AudioPacketizationCallback interface
  int32_t SendData(AudioFrameType frameType,
                   uint8_t payloadType,
                   uint32_t timeStamp,
                   const uint8_t* payloadData,
                   size_t payloadSize) override;

  rtc::ThreadChecker worker_thread_checker_;

  std::shared_ptr<RtpRtcp> rtp_rtcp_;
  std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;
  std::unique_ptr<AudioCodingModule> audio_coding_;

  // This is just an offset, RTP module will add its own random offset.
  uint32_t timestamp_ RTC_GUARDED_BY(encoder_queue_) = 0;

  bool input_mute_ = false;
  bool previous_frame_muted_ RTC_GUARDED_BY(encoder_queue_) = false;
  bool encoder_queue_is_active_ RTC_GUARDED_BY(encoder_queue_) = false;

  // Defined last to ensure that there are no running tasks when the other
  // members are destroyed.
  rtc::TaskQueue encoder_queue_;

  int send_codec_id_ = -1;
  SdpAudioFormat send_codec_spec_{"", 0, 0};

  friend class AudioEgressTest;
  friend class AudioIngressTest;
  friend class AudioChannelTest;
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_AUDIO_EGRESS_H_
