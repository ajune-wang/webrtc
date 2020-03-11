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
#include "audio/utility/audio_frame_operations.h"
#include "call/audio_sender.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

// AudioEgress receives input samples from AudioDeviceModule via
// AudioTransportImpl through AudioSender interface. Once it encodes the sample
// via selected encoder through AudioPacketizationCallback interface, the
// encoded payload will be packetized by RTP stack, resulting in ready to send
// RTP packet to remote endpoint.
//
// This class interacts with two threads; one from application and the other
// from audio input thread from AudioSender. Each method is protected by
// by SequenceChecker to ensure thread safety on members.
//
// Note that this class is originally based on ChannelSend in
// audio/channel_send.cc with non-audio related logic trimmed as aimed for
// smaller footprint.
class AudioEgress : public AudioSender, public AudioPacketizationCallback {
 public:
  AudioEgress(RtpRtcp* rtp_rtcp, Clock* clock);
  ~AudioEgress() override;

  // Set the encoder format and payload type for AudioCodingModule
  // It's possible to change the encoder type during its active usage.
  // |payload_type| must be the type that is negotiated with peer thru
  // offer/answer.
  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder);

  // Start or stop sending operation of AudioEgress. This will start/stop
  // RTP stack.
  void StartSend();
  void StopSend();

  // Query the state of RTP stack.
  // After StartSend(), this will return true, otherwise false. This class
  // doesn't carry a separate flag to check this status but rather uses
  // RTP stack's status API.
  bool IsSending() const;

  // Enable or disable Mute state
  void SetMute(bool mute);

  // Retrieve current encoder format info
  absl::optional<SdpAudioFormat> GetEncoderFormat();

  // Register the payload type and sample rate for DTMF (RFC 4733) payload.
  void RegisterTelephoneEventType(int rtp_payload_type, int sample_rate_hz);

  // Send DTMF named event as specified by
  // https://tools.ietf.org/html/rfc4733#section-3.2
  // |duration_ms| specifies the duration of DTMF packets that will be emitted
  // in place of real RTP packets instead.
  bool SendTelephoneEvent(int event, int duration_ms);

  // Implementation of AudioSender interface.
  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;

  // Implementation of AudioPacketizationCallback interface.
  int32_t SendData(AudioFrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_size) override;

 private:
  // Ensure single thread access from application.
  SequenceChecker app_thread_checker_;

  // Ensure single thread access from AudioSender
  SequenceChecker audio_thread_checker_;

  // synchronization is handled internally
  RtpRtcp* const rtp_rtcp_;

  // synchronization is handled internally
  const std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;

  // synchronization is handled internally
  const std::unique_ptr<AudioCodingModule> audio_coding_;

  // Offset used to mark rtp timestamp in sample rate unit in
  // newly received audio frame from AudioTransport.
  uint32_t next_frame_rtp_timestamp_ RTC_GUARDED_BY(audio_thread_checker_) = 0;

  // Mute flag to track mute state from application.
  // app_thread_checker_ enforces sequential write from application while audio
  // thread from AudioSender will only read from it.
  bool mute_ = false;

  // Internal mute logic uses AudioFrameOperations::Mute which requires previous
  // state of mute as part of input.
  bool previously_muted_ RTC_GUARDED_BY(audio_thread_checker_) = false;

  // A copy of encoder format that will be leveraged by VoipCore to set the
  // highest sample rate and channel to be used by AudioTransportImpl.
  absl::optional<SdpAudioFormat> encoder_format_
      RTC_GUARDED_BY(app_thread_checker_);
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_AUDIO_EGRESS_H_
