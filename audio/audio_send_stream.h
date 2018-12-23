/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_SEND_STREAM_H_
#define AUDIO_AUDIO_SEND_STREAM_H_

#include <memory>
#include <vector>

#include "audio/channel_send.h"
#include "audio/transport_feedback_packet_loss_tracker.h"
#include "call/audio_send_stream.h"
#include "call/audio_state.h"
#include "call/bitrate_allocator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class RtcEventLog;
class RtcpBandwidthObserver;
class RtcpRttStats;
class RtpTransportControllerSendInterface;

namespace internal {
class AudioState;

class AudioSendStream final : public webrtc::AudioSendStream,
                              public webrtc::BitrateAllocatorObserver,
                              public webrtc::PacketFeedbackObserver {
 public:
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                  rtc::TaskQueue* worker_queue,
                  ProcessThread* module_process_thread,
                  RtpTransportControllerSendInterface* rtp_transport,
                  BitrateAllocatorInterface* bitrate_allocator,
                  RtcEventLog* event_log,
                  RtcpRttStats* rtcp_rtt_stats,
                  const absl::optional<RtpState>& suspended_rtp_state);
  // For unit tests, which need to supply a mock ChannelSend.
  AudioSendStream(const webrtc::AudioSendStream::Config& config,
                  const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                  rtc::TaskQueue* worker_queue,
                  RtpTransportControllerSendInterface* rtp_transport,
                  BitrateAllocatorInterface* bitrate_allocator,
                  RtcEventLog* event_log,
                  RtcpRttStats* rtcp_rtt_stats,
                  const absl::optional<RtpState>& suspended_rtp_state,
                  std::unique_ptr<voe::ChannelSendInterface> channel_send);
  ~AudioSendStream() override;

  // webrtc::AudioSendStream implementation.
  const webrtc::AudioSendStream::Config& GetConfig() const override;
  void Reconfigure(const webrtc::AudioSendStream::Config& config) override;
  void Start() override;
  void Stop() override;
  void SendAudioData(std::unique_ptr<AudioFrame> audio_frame) override;
  bool SendTelephoneEvent(int payload_type,
                          int payload_frequency,
                          int event,
                          int duration_ms) override;
  void SetMuted(bool muted) override;
  webrtc::AudioSendStream::Stats GetStats() const override;
  webrtc::AudioSendStream::Stats GetStats(
      bool has_remote_tracks) const override;

  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);

  // Implements BitrateAllocatorObserver.
  uint32_t OnBitrateUpdated(BitrateAllocationUpdate update) override;

  // From PacketFeedbackObserver.
  void OnPacketAdded(uint32_t ssrc, uint16_t seq_num) override;
  void OnPacketFeedbackVector(
      const std::vector<PacketFeedback>& packet_feedback_vector) override;

  void SetTransportOverhead(int transport_overhead_per_packet_bytes);

  RtpState GetRtpState() const;
  const voe::ChannelSendInterface* GetChannel() const;

 private:
  class TimedTransport;

  static constexpr int kIpV4OverheadBytes = 20;
  static constexpr int kIpV6OverheadBytes = 40;
  static constexpr int kUdpOverheadBytes = 8;
  static constexpr int kSrtpOverheadBytes = 10;
  static constexpr int kRtpOverheadBytes = 12;

  internal::AudioState* audio_state();
  const internal::AudioState* audio_state() const;

  void StoreEncoderProperties(int sample_rate_hz,
                              size_t num_channels,
                              int min_frame_length_ms,
                              int max_frame_length_ms);

  // These are all static to make it less likely that (the old) config_ is
  // accessed unintentionally.
  static void ConfigureStream(AudioSendStream* stream,
                              const Config& new_config,
                              bool first_time);
  static bool SetupSendCodec(AudioSendStream* stream, const Config& new_config);
  static bool ReconfigureSendCodec(AudioSendStream* stream,
                                   const Config& new_config);
  static void ReconfigureANA(AudioSendStream* stream, const Config& new_config);
  static void ReconfigureCNG(AudioSendStream* stream, const Config& new_config);
  static void ReconfigureBitrateObserver(
      AudioSendStream* stream,
      const Config& new_config,
      int new_transport_overhead_per_packet_bytes);

  // Helper function to calculate minimum and maximum bitrate limits taking
  // into account overhead. If field trial WebRTC-SendSideBwe-WithOverhead is
  // not enabled, returns min and max bitrate from config_.
  void GetMinMaxBitrateWithOverhead(int* min_bitrate_bps_with_overhead,
                                    int* max_bitrate_bps_with_overhead) const;

  void ConfigureBitrateObserver(int min_bitrate_bps,
                                int max_bitrate_bps,
                                double bitrate_priority,
                                bool has_packet_feedback,
                                int transport_overhead_per_packet_bytes);
  void RemoveBitrateObserver();

  void RegisterCngPayloadType(int payload_type, int clockrate_hz);

  // Field trial WebRTC-SendSideBwe-WithOverhead.
  //
  // Adds hardcoded overhead bitrate to both min_bitrate_bps and
  // max_bitrate_bps, based on 50 byte overhead (ipv4, no TURN) and opus
  // maximum frame size 60ms or 120ms.
  const bool send_side_bwe_with_overhead_;

  // Field trial WebRTC-SendSideBwe-WithOverheadOptionMinMax.
  //
  // Option for WebRTC-SendSideBwe-WithOverhead to add minimum overhead
  // to min_bitrate_bps and maximum overhead to max_bitrate_bps. The overhead
  // is calculated based on current transport and packetization overhead and
  // minimum / maximum supported encoder frame size.
  const bool send_side_bwe_with_overhead_option_min_max_;

  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker pacer_thread_checker_;
  rtc::RaceChecker audio_capture_race_checker_;
  rtc::TaskQueue* worker_queue_;
  webrtc::AudioSendStream::Config config_;

  // Most recent transport packet overhead.
  // Default overhead (until we get overhead notification callback) is based on
  // RTP transport overhead with IPV6 without TURN.
  int transport_overhead_per_packet_bytes_ =
      kIpV6OverheadBytes + kUdpOverheadBytes + kSrtpOverheadBytes +
      kRtpOverheadBytes;

  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  const std::unique_ptr<voe::ChannelSendInterface> channel_send_;
  RtcEventLog* const event_log_;

  int encoder_sample_rate_hz_ = 0;
  size_t encoder_num_channels_ = 0;
  bool sending_ = false;

  // Minimum and maximum frame length supported by the encoder.
  // Used to estimate overhead bitrate.
  int encoder_min_frame_length_ms_ = 0;
  int encoder_max_frame_length_ms_ = 0;

  BitrateAllocatorInterface* const bitrate_allocator_;
  RtpTransportControllerSendInterface* const rtp_transport_;

  rtc::CriticalSection packet_loss_tracker_cs_;
  TransportFeedbackPacketLossTracker packet_loss_tracker_
      RTC_GUARDED_BY(&packet_loss_tracker_cs_);

  RtpRtcp* rtp_rtcp_module_;
  absl::optional<RtpState> const suspended_rtp_state_;

  // RFC 5285: Each distinct extension MUST have a unique ID. The value 0 is
  // reserved for padding and MUST NOT be used as a local identifier.
  // So it should be safe to use 0 here to indicate "not configured".
  struct ExtensionIds {
    int audio_level = 0;
    int transport_sequence_number = 0;
    int mid = 0;
  };
  static ExtensionIds FindExtensionIds(
      const std::vector<RtpExtension>& extensions);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioSendStream);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_SEND_STREAM_H_
