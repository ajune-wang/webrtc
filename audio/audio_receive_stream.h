/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_RECEIVE_STREAM_H_
#define AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/call/audio_sink.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/frame_transformer_interface.h"
#include "api/neteq/neteq_factory.h"
#include "api/rtp_headers.h"
#include "api/sequence_checker.h"
#include "api/transport/rtp/rtp_source.h"
#include "audio/audio_level.h"
#include "audio/audio_state.h"
#include "audio/channel_receive_frame_transformer_delegate.h"
#include "audio/channel_send.h"
#include "call/audio_receive_stream.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/source/absolute_capture_time_interpolator.h"
#include "modules/rtp_rtcp/source/capture_clock_offset_updater.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "modules/rtp_rtcp/source/source_tracker.h"
#include "rtc_base/system/no_unique_address.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
class PacketRouter;
class ProcessThread;
class RtcEventLog;
class RtpPacketReceived;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

namespace internal {
class AudioSendStream;

class AudioReceiveStream final : public webrtc::AudioReceiveStream,
                                 public AudioMixer::Source,
                                 public Syncable,
                                 public RtcpPacketTypeCounterObserver,
                                 public RtpPacketSinkInterface {
 public:
  AudioReceiveStream(Clock* clock,
                     PacketRouter* packet_router,
                     NetEqFactory* neteq_factory,
                     const webrtc::AudioReceiveStream::Config& config,
                     const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
                     webrtc::RtcEventLog* event_log);

  AudioReceiveStream() = delete;
  AudioReceiveStream(const AudioReceiveStream&) = delete;
  AudioReceiveStream& operator=(const AudioReceiveStream&) = delete;

  // Destruction happens on the worker thread. Prior to destruction the caller
  // must ensure that a registration with the transport has been cleared. See
  // `RegisterWithTransport` for details.
  // TODO(tommi): As a further improvement to this, performing the full
  // destruction on the network thread could be made the default.
  ~AudioReceiveStream() override;

  // Called on the network thread to register/unregister with the network
  // transport.
  void RegisterWithTransport(
      RtpStreamReceiverControllerInterface* receiver_controller);
  // If registration has previously been done (via `RegisterWithTransport`) then
  // `UnregisterFromTransport` must be called prior to destruction, on the
  // network thread.
  void UnregisterFromTransport();

  // webrtc::AudioReceiveStream implementation.
  void Start() override;
  void Stop() override;
  const RtpConfig& rtp_config() const override { return config_.rtp; }
  bool IsRunning() const override;
  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      override;
  void SetDecoderMap(std::map<int, SdpAudioFormat> decoder_map) override;
  void SetUseTransportCcAndNackHistory(bool use_transport_cc,
                                       int history_ms) override;
  void SetFrameDecryptor(rtc::scoped_refptr<webrtc::FrameDecryptorInterface>
                             frame_decryptor) override;
  void SetRtpExtensions(std::vector<RtpExtension> extensions) override;

  webrtc::AudioReceiveStream::Stats GetStats(
      bool get_and_clear_legacy_stats) const override;
  void SetSink(AudioSinkInterface* sink) override;
  void SetGain(float gain) override;
  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override;
  int GetBaseMinimumPlayoutDelayMs() const override;
  std::vector<webrtc::RtpSource> GetSources() const override;

  // AudioMixer::Source
  AudioFrameInfo GetAudioFrameWithInfo(int sample_rate_hz,
                                       AudioFrame* audio_frame) override;
  int Ssrc() const override;
  int PreferredSampleRate() const override;

  // Syncable
  uint32_t id() const override;
  absl::optional<Syncable::Info> GetInfo() const override;
  bool GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                              int64_t* time_ms) const override;
  void SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                         int64_t time_ms) override;
  bool SetMinimumPlayoutDelay(int delay_ms) override;

  void AssociateSendStream(AudioSendStream* send_stream);
  void DeliverRtcp(const uint8_t* packet, size_t length);

  void SetSyncGroup(const std::string& sync_group);

  void SetLocalSsrc(uint32_t local_ssrc);

  uint32_t local_ssrc() const;

  uint32_t remote_ssrc() const {
    // The remote_ssrc member variable of config_ will never change and can be
    // considered const.
    return config_.rtp.remote_ssrc;
  }

  const webrtc::AudioReceiveStream::Config& config() const;
  const AudioSendStream* GetAssociatedSendStreamForTesting() const;

  // RtpPacketSinkInterface.
  void OnRtpPacket(const RtpPacketReceived& packet) override;
  // RtcpPacketTypeCounterObserver.
  void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) override;

  // TODO(tommi): Remove this method.
  void ReconfigureForTesting(const webrtc::AudioReceiveStream::Config& config);

 private:
  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs);

  // Audio+Video Sync.
  absl::optional<int64_t> GetCurrentEstimatedPlayoutNtpTimestampMs(
      int64_t now_ms) const;

  void RegisterReceiverCongestionControlObjects(PacketRouter* packet_router);
  void ResetReceiverCongestionControlObjects();

  void SetNACKStatus(bool enable, int maxNumberOfPackets);

  AudioState* audio_state() const;

  void ReceivePacket(const uint8_t* packet,
                     size_t packet_length,
                     const RTPHeader& header)
      RTC_RUN_ON(worker_thread_checker_);
  void UpdatePlayoutTimestamp(bool rtcp, int64_t now_ms)
      RTC_RUN_ON(worker_thread_checker_);

  int GetRtpTimestampRateHz() const;
  int64_t GetRTT() const;

  void OnReceivedPayloadData(rtc::ArrayView<const uint8_t> payload,
                             const RTPHeader& rtpHeader)
      RTC_RUN_ON(worker_thread_checker_);

  void InitFrameTransformerDelegate(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer)
      RTC_RUN_ON(worker_thread_checker_);

  // Thread checkers document and lock usage of some methods to specific threads
  // we know about. The goal is to eventually split up voe::ChannelReceive into
  // parts with single-threaded semantics, and thereby reduce the need for
  // locks.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker network_thread_checker_;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_thread_checker_;
  // TODO(bugs.webrtc.org/11993): This checker conceptually represents
  // operations that belong to the network thread. The Call class is currently
  // moving towards handling network packets on the network thread and while
  // that work is ongoing, this checker may in practice represent the worker
  // thread, but still serves as a mechanism of grouping together concepts
  // that belong to the network thread. Once the packets are fully delivered
  // on the network thread, this comment will be deleted.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_;
  webrtc::AudioReceiveStream::Config config_;
  Clock* const clock_;
  rtc::scoped_refptr<webrtc::AudioState> audio_state_;
  SourceTracker source_tracker_;
  AudioSendStream* associated_send_stream_
      RTC_GUARDED_BY(packet_sequence_checker_) = nullptr;

  bool playing_ RTC_GUARDED_BY(worker_thread_checker_) = false;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);

  TaskQueueBase* const worker_thread_;
  ScopedTaskSafety worker_safety_;

  // Methods accessed from audio and video threads are checked for sequential-
  // only access. We don't necessarily own and control these threads, so thread
  // checkers cannot be used. E.g. Chromium may transfer "ownership" from one
  // audio thread to another, but access is still sequential.
  rtc::RaceChecker audio_thread_race_checker_;
  Mutex callback_mutex_;
  Mutex volume_settings_mutex_;

  RtcEventLog* const event_log_;

  // Indexed by payload type.
  std::map<uint8_t, int> payload_type_frequencies_;

  std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;
  const uint32_t remote_ssrc_;

  // Info for GetSyncInfo is updated on network or worker thread, and queried on
  // the worker thread.
  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(&worker_thread_checker_);
  absl::optional<int64_t> last_received_rtp_system_time_ms_
      RTC_GUARDED_BY(&worker_thread_checker_);

  // The AcmReceiver is thread safe, using its own lock.
  acm2::AcmReceiver acm_receiver_;
  AudioSinkInterface* audio_sink_ = nullptr;
  voe::AudioLevel _outputAudioLevel;

  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(ts_stats_lock_);

  // Timestamp of the audio pulled from NetEq.
  absl::optional<uint32_t> jitter_buffer_playout_timestamp_;

  uint32_t playout_timestamp_rtp_ RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_rtp_time_ms_
      RTC_GUARDED_BY(worker_thread_checker_);
  uint32_t playout_delay_ms_ RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_ntp_
      RTC_GUARDED_BY(worker_thread_checker_);
  absl::optional<int64_t> playout_timestamp_ntp_time_ms_
      RTC_GUARDED_BY(worker_thread_checker_);

  mutable Mutex ts_stats_lock_;

  std::unique_ptr<rtc::TimestampWrapAroundHandler> rtp_ts_wraparound_handler_;
  // The rtp timestamp of the first played out audio frame.
  int64_t capture_start_rtp_time_stamp_;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_ RTC_GUARDED_BY(ts_stats_lock_);

  AudioDeviceModule* _audioDeviceModulePtr;
  float _outputGain RTC_GUARDED_BY(volume_settings_mutex_);

  const voe::ChannelSendInterface* associated_send_channel_
      RTC_GUARDED_BY(network_thread_checker_);

  PacketRouter* packet_router_ = nullptr;

  // E2EE Audio Frame Decryption
  rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor_
      RTC_GUARDED_BY(worker_thread_checker_);
  webrtc::CryptoOptions crypto_options_;

  webrtc::AbsoluteCaptureTimeInterpolator absolute_capture_time_interpolator_
      RTC_GUARDED_BY(worker_thread_checker_);

  webrtc::CaptureClockOffsetUpdater capture_clock_offset_updater_;

  rtc::scoped_refptr<ChannelReceiveFrameTransformerDelegate>
      frame_transformer_delegate_;

  // Counter that's used to control the frequency of reporting histograms
  // from the `GetAudioFrameWithInfo` callback.
  int audio_frame_interval_count_ RTC_GUARDED_BY(audio_thread_race_checker_) =
      0;
  // Controls how many callbacks we let pass by before reporting callback stats.
  // A value of 100 means 100 callbacks, each one of which represents 10ms worth
  // of data, so the stats reporting frequency will be 1Hz (modulo failures).
  constexpr static int kHistogramReportingInterval = 100;

  mutable Mutex rtcp_counter_mutex_;
  RtcpPacketTypeCounter rtcp_packet_type_counter_
      RTC_GUARDED_BY(rtcp_counter_mutex_);
};
}  // namespace internal
}  // namespace webrtc

#endif  // AUDIO_AUDIO_RECEIVE_STREAM_H_
