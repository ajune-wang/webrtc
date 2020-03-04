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

// Audio logic can be divided into two directions. Incoming (ingress) or
// outgoing (egress), thus we have AudioEgress that handles outgoing logic. It's
// possible to implement both logics using single class but that wouldn't
// provide desired the clarity and cleaness of the code.  Thus, it makes sense
// to divide this into two separate classes.
//
// AudioEgress inherits two class AudioSender and AudioPacketizationcallback.
// AudioSender is the interface for AudioTransportimpl to feed audio samples
// whereas AudioPacketizationcallback is interface for AudioCodingModule to call
// back with completed encoded payload using chosen encoder. Underneath the
// hood, AudioEgress receives input samples from AudioDeviceModule via
// AudioTransportImpl. Once it encodes the sample via selected encoder, the
// encoded payload will be packetized by RTP stack, resulting in ready to send
// RTP packet to remote endpoint.
//
// Note that this class is originally based on ChannelSend in
// audio/channel_send.cc.  It would be ideal to refactor the common logic so
// that we don't have duplicate logic.  However, for simplicity and flexibility
// for audio only API, it deemed to be better to trim off all audio unrelated
// logic to server the purpose.
class AudioEgress : public AudioSender, public AudioPacketizationCallback {
 public:
  AudioEgress(RtpRtcp* rtp_rtcp,
              Clock* clock,
              TaskQueueFactory* task_queue_factory);
  ~AudioEgress() override;

  // Set the encoder format and payload type for AudioCodingModule
  // It's possible to change the encoder type during its active usage.
  // |payload_type| must be the type that is negotiated with peer thru
  // offer/answer.
  void SetEncoder(int payload_type,
                  const SdpAudioFormat& encoder_format,
                  std::unique_ptr<AudioEncoder> encoder);

  // Start or stop sending operation of AudioEgress. This will start/stop
  // RTP stack also encoder queue thread to be active.
  void StartSend();
  void StopSend();

  // Query the state of RTP stack.
  // After StartSend(), this will return true, otherwise false. This class
  // doesn't carry a separate flag to check this status but rather uses
  // RTP stack's status API.
  bool IsSending() const;

  // Enable or disable Mute state
  void SetMute(bool mute);

  // Retrieve current encoder related info
  int EncoderSampleRate();
  size_t EncoderNumChannel();

  // Register DTMF (RFC 4733) payload type and its sampling rate.
  // Payload type and its sampling rate must be what was negotiated with peer.
  void RegisterTelephoneEventType(int rtp_payload_type, int sampling_rate_hz);

  // Send DTMF named event in outband mode as specific by
  // https://tools.ietf.org/html/rfc4733#page-25
  // |duration_ms| specifices the number of DTMF packets that will be emitted
  // in provided period in milliseconds.
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
  // Ensure that single worker thread access.
  rtc::ThreadChecker worker_thread_checker_;

  // RtpRtcp, RTPSenderAudio, AudioCodingModule employ mutex internally to
  // ensure reentrancy on both TaskQueue (encoder_queue_) and application
  // threads.
  RtpRtcp* const rtp_rtcp_ = nullptr;
  const std::unique_ptr<RTPSenderAudio> rtp_sender_audio_;
  const std::unique_ptr<AudioCodingModule> audio_coding_;

  // Offset used to mark rtp timestamp in sampling rate unit in
  // newly received audio frame from AudioTransport.
  uint32_t rtp_timestamp_offset_ RTC_GUARDED_BY(encoder_queue_) = 0;

  // Concurrent write on mute_ is protected by worker_thread_checker_
  // while read on mute_ is done by encoder_queue_.
  bool mute_ = false;
  bool previously_muted_ RTC_GUARDED_BY(encoder_queue_) = false;
  bool active_encoder_queue_ RTC_GUARDED_BY(encoder_queue_) = false;

  absl::optional<SdpAudioFormat> encoder_format_
      RTC_GUARDED_BY(worker_thread_checker_);

  // Defined last to ensure that there are no running tasks when the other
  // members are destroyed.
  rtc::TaskQueue encoder_queue_;
};

}  // namespace webrtc

#endif  // AUDIO_VOIP_AUDIO_EGRESS_H_
