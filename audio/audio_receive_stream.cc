/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_receive_stream.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/call/audio_sink.h"
#include "api/rtp_parameters.h"
#include "api/sequence_checker.h"
#include "audio/audio_send_stream.h"
#include "audio/audio_state.h"
#include "audio/conversion.h"
#include "audio/utility/audio_frame_operations.h"
#include "call/rtp_config.h"
#include "call/rtp_stream_receiver_controller_interface.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

constexpr double kAudioSampleDurationSeconds = 0.01;

// Video Sync.
constexpr int kVoiceEngineMinMinPlayoutDelayMs = 0;
constexpr int kVoiceEngineMaxMinPlayoutDelayMs = 10000;

AudioCodingModule::Config AcmConfig(
    NetEqFactory* neteq_factory,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
    absl::optional<AudioCodecPairId> codec_pair_id,
    size_t jitter_buffer_max_packets,
    bool jitter_buffer_fast_accelerate) {
  AudioCodingModule::Config acm_config;
  acm_config.neteq_factory = neteq_factory;
  acm_config.decoder_factory = decoder_factory;
  acm_config.neteq_config.codec_pair_id = codec_pair_id;
  acm_config.neteq_config.max_packets_in_buffer = jitter_buffer_max_packets;
  acm_config.neteq_config.enable_fast_accelerate =
      jitter_buffer_fast_accelerate;
  acm_config.neteq_config.enable_muted_state = true;

  return acm_config;
}

}  // namespace

std::string AudioReceiveStream::Config::Rtp::ToString() const {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
  ss << ", nack: " << nack.ToString();
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "{rtp: " << rtp.ToString();
  ss << ", rtcp_send_transport: "
     << (rtcp_send_transport ? "(Transport)" : "null");
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << '}';
  return ss.str();
}

namespace internal {

AudioReceiveStream::AudioReceiveStream(
    Clock* clock,
    PacketRouter* packet_router,
    NetEqFactory* neteq_factory,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state,
    webrtc::RtcEventLog* event_log)
    : config_(config),
      clock_(clock),
      audio_state_(audio_state),
      source_tracker_(clock),
      worker_thread_(TaskQueueBase::Current()),
      event_log_(event_log),
      rtp_receive_statistics_(ReceiveStatistics::Create(clock)),
      remote_ssrc_(config.rtp.remote_ssrc),
      acm_receiver_(AcmConfig(neteq_factory,
                              config.decoder_factory,
                              config.codec_pair_id,
                              config.jitter_buffer_max_packets,
                              config.jitter_buffer_fast_accelerate)),
      _outputAudioLevel(),
      ntp_estimator_(clock),
      playout_timestamp_rtp_(0),
      playout_delay_ms_(0),
      rtp_ts_wraparound_handler_(new rtc::TimestampWrapAroundHandler()),
      capture_start_rtp_time_stamp_(-1),
      capture_start_ntp_time_ms_(-1),
      _audioDeviceModulePtr(
          static_cast<internal::AudioState*>(audio_state.get())
              ->audio_device_module()),
      _outputGain(1.0f),
      associated_send_channel_(nullptr),
      frame_decryptor_(config.frame_decryptor),
      crypto_options_(config.crypto_options),
      absolute_capture_time_interpolator_(clock) {
  RTC_LOG(LS_INFO) << "AudioReceiveStream: " << config.rtp.remote_ssrc;
  RTC_DCHECK(config.decoder_factory);
  RTC_DCHECK(config.rtcp_send_transport);
  RTC_DCHECK(audio_state_);
  RTC_DCHECK(packet_router);

  packet_sequence_checker_.Detach();
  network_thread_checker_.Detach();

  acm_receiver_.ResetInitialDelay();
  acm_receiver_.SetMinimumDelay(0);
  acm_receiver_.SetMaximumDelay(0);
  acm_receiver_.FlushBuffers();

  _outputAudioLevel.ResetLevelFullRange();

  rtp_receive_statistics_->EnableRetransmitDetection(remote_ssrc_, true);
  RtpRtcpInterface::Configuration configuration;
  configuration.clock = clock;
  configuration.audio = true;
  configuration.receiver_only = true;
  configuration.outgoing_transport = config.rtcp_send_transport;
  configuration.receive_statistics = rtp_receive_statistics_.get();
  configuration.event_log = event_log_;
  configuration.local_media_ssrc = config.rtp.local_ssrc;
  configuration.rtcp_packet_type_counter_observer = this;

  if (config.frame_transformer)
    InitFrameTransformerDelegate(std::move(config.frame_transformer));

  rtp_rtcp_ = ModuleRtpRtcpImpl2::Create(configuration);
  rtp_rtcp_->SetSendingMediaStatus(false);
  rtp_rtcp_->SetRemoteSSRC(remote_ssrc_);

  // Ensure that RTCP is enabled for the created channel.
  rtp_rtcp_->SetRTCPStatus(RtcpMode::kCompound);
  // Configure bandwidth estimation.
  RegisterReceiverCongestionControlObjects(packet_router);

  // Complete configuration.
  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  SetNACKStatus(config.rtp.nack.rtp_history_ms != 0,
                config.rtp.nack.rtp_history_ms / 20);
  SetReceiveCodecs(config.decoder_map);
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_LOG(LS_INFO) << "~AudioReceiveStream: " << config_.rtp.remote_ssrc;
  Stop();
  ResetReceiverCongestionControlObjects();

  // Resets the delegate's callback to ChannelReceive::OnReceivedPayloadData.
  if (frame_transformer_delegate_)
    frame_transformer_delegate_->Reset();
}

void AudioReceiveStream::RegisterWithTransport(
    RtpStreamReceiverControllerInterface* receiver_controller) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  RTC_DCHECK(!rtp_stream_receiver_);
  rtp_stream_receiver_ =
      receiver_controller->CreateReceiver(config_.rtp.remote_ssrc, this);
}

void AudioReceiveStream::UnregisterFromTransport() {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  rtp_stream_receiver_.reset();
}

void AudioReceiveStream::ReconfigureForTesting(
    const webrtc::AudioReceiveStream::Config& config) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  // SSRC can't be changed mid-stream.
  RTC_DCHECK_EQ(config_.rtp.remote_ssrc, config.rtp.remote_ssrc);
  RTC_DCHECK_EQ(config_.rtp.local_ssrc, config.rtp.local_ssrc);

  // Configuration parameters which cannot be changed.
  RTC_DCHECK_EQ(config_.rtcp_send_transport, config.rtcp_send_transport);
  // Decoder factory cannot be changed because it is configured at
  // voe::Channel construction time.
  RTC_DCHECK_EQ(config_.decoder_factory, config.decoder_factory);

  // TODO(solenberg): Config NACK history window (which is a packet count),
  // using the actual packet size for the configured codec.
  RTC_DCHECK_EQ(config_.rtp.nack.rtp_history_ms, config.rtp.nack.rtp_history_ms)
      << "Use SetUseTransportCcAndNackHistory";

  RTC_DCHECK(config_.decoder_map == config.decoder_map) << "Use SetDecoderMap";
  RTC_DCHECK_EQ(config_.frame_transformer, config.frame_transformer)
      << "Use SetDepacketizerToDecoderFrameTransformer";

  config_ = config;
}

void AudioReceiveStream::Start() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (playing_) {
    return;
  }
  playing_ = true;
  audio_state()->AddReceivingStream(this);
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playing_) {
    return;
  }
  _outputAudioLevel.ResetLevelFullRange();
  playing_ = false;
  audio_state()->RemoveReceivingStream(this);
}

bool AudioReceiveStream::IsRunning() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return playing_;
}

void AudioReceiveStream::SetDepacketizerToDecoderFrameTransformer(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // Depending on when the channel is created, the transformer might be set
  // twice. Don't replace the delegate if it was already initialized.
  if (!frame_transformer || frame_transformer_delegate_) {
    RTC_NOTREACHED() << "Not setting the transformer?";
    return;
  }

  InitFrameTransformerDelegate(std::move(frame_transformer));
}

void AudioReceiveStream::SetDecoderMap(
    std::map<int, SdpAudioFormat> decoder_map) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  config_.decoder_map = std::move(decoder_map);
  SetReceiveCodecs(config_.decoder_map);
}

void AudioReceiveStream::SetUseTransportCcAndNackHistory(bool use_transport_cc,
                                                         int history_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_GE(history_ms, 0);
  config_.rtp.transport_cc = use_transport_cc;
  if (config_.rtp.nack.rtp_history_ms != history_ms) {
    config_.rtp.nack.rtp_history_ms = history_ms;
    // TODO(solenberg): Config NACK history window (which is a packet count),
    // using the actual packet size for the configured codec.
    SetNACKStatus(history_ms != 0, history_ms / 20);
  }
}

void AudioReceiveStream::SetFrameDecryptor(
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  // TODO(bugs.webrtc.org/11993): This is called via WebRtcAudioReceiveStream,
  // expect to be called on the network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  frame_decryptor_ = std::move(frame_decryptor);
}

void AudioReceiveStream::SetRtpExtensions(
    std::vector<RtpExtension> extensions) {
  // TODO(bugs.webrtc.org/11993): This is called via WebRtcAudioReceiveStream,
  // expect to be called on the network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  config_.rtp.extensions = std::move(extensions);
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats(
    bool get_and_clear_legacy_stats) const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  webrtc::AudioReceiveStream::Stats stats;
  stats.remote_ssrc = config_.rtp.remote_ssrc;

  // The jitter statistics is updated for each received RTP packet and is based
  // on received packets.
  RtpReceiveStats rtp_stats;
  StreamStatistician* statistician =
      rtp_receive_statistics_->GetStatistician(remote_ssrc_);
  if (statistician) {
    rtp_stats = statistician->GetStats();
  }

  stats.packets_lost = rtp_stats.packets_lost;

  // Data counters.
  if (statistician) {
    stats.payload_bytes_rcvd = rtp_stats.packet_counter.payload_bytes;

    stats.header_and_padding_bytes_rcvd =
        rtp_stats.packet_counter.header_bytes +
        rtp_stats.packet_counter.padding_bytes;
    stats.packets_rcvd = rtp_stats.packet_counter.packets;
    stats.last_packet_received_timestamp_ms =
        rtp_stats.last_packet_received_timestamp_ms;
  }

  {
    MutexLock lock(&rtcp_counter_mutex_);
    stats.nacks_sent = rtcp_packet_type_counter_.nack_packets;
  }

  // Timestamps.
  {
    MutexLock lock(&ts_stats_lock_);
    stats.capture_start_ntp_time_ms = capture_start_ntp_time_ms_;
  }

  absl::optional<RtpRtcpInterface::SenderReportStats> rtcp_sr_stats =
      rtp_rtcp_->GetSenderReportStats();
  if (rtcp_sr_stats.has_value()) {
    // Number of seconds since 1900 January 1 00:00 GMT (see
    // https://tools.ietf.org/html/rfc868).
    constexpr int64_t kNtpJan1970Millisecs =
        2208988800 * rtc::kNumMillisecsPerSec;
    stats.last_sender_report_timestamp_ms =
        rtcp_sr_stats->last_arrival_timestamp.ToMs() - kNtpJan1970Millisecs;
    stats.last_sender_report_remote_timestamp_ms =
        rtcp_sr_stats->last_remote_timestamp.ToMs() - kNtpJan1970Millisecs;
    stats.sender_reports_packets_sent = rtcp_sr_stats->packets_sent;
    stats.sender_reports_bytes_sent = rtcp_sr_stats->bytes_sent;
    stats.sender_reports_reports_count = rtcp_sr_stats->reports_count;
  }

  // TODO(solenberg): Don't return here if we can't get the codec - return the
  //                  stats we *can* get.
  auto receive_codec = acm_receiver_.LastDecoder();
  if (!receive_codec) {
    return stats;
  }

  stats.codec_name = receive_codec->second.name;
  stats.codec_payload_type = receive_codec->first;
  int clockrate_khz = receive_codec->second.clockrate_hz / 1000;
  if (clockrate_khz > 0) {
    stats.jitter_ms = rtp_stats.jitter / clockrate_khz;
  }
  stats.delay_estimate_ms =
      acm_receiver_.FilteredCurrentDelayMs() + playout_delay_ms_;
  stats.audio_level = _outputAudioLevel.LevelFullRange();
  stats.total_output_energy = _outputAudioLevel.TotalEnergy();
  stats.total_output_duration = _outputAudioLevel.TotalDuration();
  stats.estimated_playout_ntp_timestamp_ms =
      GetCurrentEstimatedPlayoutNtpTimestampMs(rtc::TimeMillis());

  // Get jitter buffer and total delay (alg + jitter + playout) stats.
  NetworkStatistics ns;
  acm_receiver_.GetNetworkStatistics(&ns, get_and_clear_legacy_stats);
  stats.fec_packets_received = ns.fecPacketsReceived;
  stats.fec_packets_discarded = ns.fecPacketsDiscarded;
  stats.jitter_buffer_ms = ns.currentBufferSize;
  stats.jitter_buffer_preferred_ms = ns.preferredBufferSize;
  stats.total_samples_received = ns.totalSamplesReceived;
  stats.concealed_samples = ns.concealedSamples;
  stats.silent_concealed_samples = ns.silentConcealedSamples;
  stats.concealment_events = ns.concealmentEvents;
  stats.jitter_buffer_delay_seconds =
      static_cast<double>(ns.jitterBufferDelayMs) /
      static_cast<double>(rtc::kNumMillisecsPerSec);
  stats.jitter_buffer_emitted_count = ns.jitterBufferEmittedCount;
  stats.jitter_buffer_target_delay_seconds =
      static_cast<double>(ns.jitterBufferTargetDelayMs) /
      static_cast<double>(rtc::kNumMillisecsPerSec);
  stats.inserted_samples_for_deceleration = ns.insertedSamplesForDeceleration;
  stats.removed_samples_for_acceleration = ns.removedSamplesForAcceleration;
  stats.expand_rate = Q14ToFloat(ns.currentExpandRate);
  stats.speech_expand_rate = Q14ToFloat(ns.currentSpeechExpandRate);
  stats.secondary_decoded_rate = Q14ToFloat(ns.currentSecondaryDecodedRate);
  stats.secondary_discarded_rate = Q14ToFloat(ns.currentSecondaryDiscardedRate);
  stats.accelerate_rate = Q14ToFloat(ns.currentAccelerateRate);
  stats.preemptive_expand_rate = Q14ToFloat(ns.currentPreemptiveRate);
  stats.jitter_buffer_flushes = ns.packetBufferFlushes;
  stats.delayed_packet_outage_samples = ns.delayedPacketOutageSamples;
  stats.relative_packet_arrival_delay_seconds =
      static_cast<double>(ns.relativePacketArrivalDelayMs) /
      static_cast<double>(rtc::kNumMillisecsPerSec);
  stats.interruption_count = ns.interruptionCount;
  stats.total_interruption_duration_ms = ns.totalInterruptionDurationMs;

  AudioDecodingCallStats ds;
  acm_receiver_.GetDecodingCallStatistics(&ds);
  stats.decoding_calls_to_silence_generator = ds.calls_to_silence_generator;
  stats.decoding_calls_to_neteq = ds.calls_to_neteq;
  stats.decoding_normal = ds.decoded_normal;
  stats.decoding_plc = ds.decoded_neteq_plc;
  stats.decoding_codec_plc = ds.decoded_codec_plc;
  stats.decoding_cng = ds.decoded_cng;
  stats.decoding_plc_cng = ds.decoded_plc_cng;
  stats.decoding_muted_output = ds.decoded_muted_output;

  return stats;
}

void AudioReceiveStream::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  MutexLock lock(&callback_mutex_);
  audio_sink_ = sink;
}

void AudioReceiveStream::SetGain(float gain) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  MutexLock lock(&volume_settings_mutex_);
  _outputGain = gain;
}

bool AudioReceiveStream::SetBaseMinimumPlayoutDelayMs(int delay_ms) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return acm_receiver_.SetBaseMinimumDelayMs(delay_ms);
}

int AudioReceiveStream::GetBaseMinimumPlayoutDelayMs() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return acm_receiver_.GetBaseMinimumDelayMs();
}

std::vector<RtpSource> AudioReceiveStream::GetSources() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return source_tracker_.GetSources();
}

AudioMixer::Source::AudioFrameInfo AudioReceiveStream::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  audio_frame->sample_rate_hz_ = sample_rate_hz;

  event_log_->Log(std::make_unique<RtcEventAudioPlayout>(remote_ssrc_));

  // Get 10ms raw PCM data from the ACM (mixer limits output frequency)
  bool muted;
  if (acm_receiver_.GetAudio(audio_frame->sample_rate_hz_, audio_frame,
                             &muted) == -1) {
    RTC_DLOG(LS_ERROR)
        << "AudioReceiveStream::GetAudioFrame() PlayoutData10Ms() failed!";
    // In all likelihood, the audio in this frame is garbage. We return an
    // error so that the audio mixer module doesn't add it to the mix. As
    // a result, it won't be played out and the actions skipped here are
    // irrelevant.
    return AudioMixer::Source::AudioFrameInfo::kError;
  }

  if (muted) {
    // TODO(henrik.lundin): We should be able to do better than this. But we
    // will have to go through all the cases below where the audio samples may
    // be used, and handle the muted case in some way.
    AudioFrameOperations::Mute(audio_frame);
  }

  {
    // Pass the audio buffers to an optional sink callback, before applying
    // scaling/panning, as that applies to the mix operation.
    // External recipients of the audio (e.g. via AudioTrack), will do their
    // own mixing/dynamic processing.
    MutexLock lock(&callback_mutex_);
    if (audio_sink_) {
      AudioSinkInterface::Data data(
          audio_frame->data(), audio_frame->samples_per_channel_,
          audio_frame->sample_rate_hz_, audio_frame->num_channels_,
          audio_frame->timestamp_);
      audio_sink_->OnData(data);
    }
  }

  float output_gain = 1.0f;
  {
    MutexLock lock(&volume_settings_mutex_);
    output_gain = _outputGain;
  }

  // Output volume scaling
  if (output_gain < 0.99f || output_gain > 1.01f) {
    // TODO(solenberg): Combine with mute state - this can cause clicks!
    AudioFrameOperations::ScaleWithSat(output_gain, audio_frame);
  }

  // Measure audio level (0-9)
  // TODO(henrik.lundin) Use the |muted| information here too.
  // TODO(deadbeef): Use RmsLevel for |_outputAudioLevel| (see
  // https://crbug.com/webrtc/7517).
  _outputAudioLevel.ComputeLevel(*audio_frame, kAudioSampleDurationSeconds);

  if (capture_start_rtp_time_stamp_ < 0 && audio_frame->timestamp_ != 0) {
    // The first frame with a valid rtp timestamp.
    capture_start_rtp_time_stamp_ = audio_frame->timestamp_;
  }

  if (capture_start_rtp_time_stamp_ >= 0) {
    // audio_frame.timestamp_ should be valid from now on.

    // Compute elapsed time.
    int64_t unwrap_timestamp =
        rtp_ts_wraparound_handler_->Unwrap(audio_frame->timestamp_);
    audio_frame->elapsed_time_ms_ =
        (unwrap_timestamp - capture_start_rtp_time_stamp_) /
        (GetRtpTimestampRateHz() / 1000);

    {
      MutexLock lock(&ts_stats_lock_);
      // Compute ntp time.
      audio_frame->ntp_time_ms_ =
          ntp_estimator_.Estimate(audio_frame->timestamp_);
      // |ntp_time_ms_| won't be valid until at least 2 RTCP SRs are received.
      if (audio_frame->ntp_time_ms_ > 0) {
        // Compute |capture_start_ntp_time_ms_| so that
        // |capture_start_ntp_time_ms_| + |elapsed_time_ms_| == |ntp_time_ms_|
        capture_start_ntp_time_ms_ =
            audio_frame->ntp_time_ms_ - audio_frame->elapsed_time_ms_;
      }
    }
  }

  // Fill in local capture clock offset in |audio_frame->packet_infos_|.
  RtpPacketInfos::vector_type packet_infos;
  for (auto& packet_info : audio_frame->packet_infos_) {
    absl::optional<int64_t> local_capture_clock_offset;
    if (packet_info.absolute_capture_time().has_value()) {
      local_capture_clock_offset =
          capture_clock_offset_updater_.AdjustEstimatedCaptureClockOffset(
              packet_info.absolute_capture_time()
                  ->estimated_capture_clock_offset);
    }
    RtpPacketInfo new_packet_info(packet_info);
    new_packet_info.set_local_capture_clock_offset(local_capture_clock_offset);
    packet_infos.push_back(std::move(new_packet_info));
  }
  audio_frame->packet_infos_ = RtpPacketInfos(packet_infos);

  ++audio_frame_interval_count_;
  if (audio_frame_interval_count_ >= kHistogramReportingInterval) {
    audio_frame_interval_count_ = 0;
    worker_thread_->PostTask(ToQueuedTask(worker_safety_, [this]() {
      RTC_DCHECK_RUN_ON(&worker_thread_checker_);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.TargetJitterBufferDelayMs",
                                acm_receiver_.TargetDelayMs());
      const int jitter_buffer_delay = acm_receiver_.FilteredCurrentDelayMs();
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDelayEstimateMs",
                                jitter_buffer_delay + playout_delay_ms_);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverJitterBufferDelayMs",
                                jitter_buffer_delay);
      RTC_HISTOGRAM_COUNTS_1000("WebRTC.Audio.ReceiverDeviceDelayMs",
                                playout_delay_ms_);
    }));
  }

  source_tracker_.OnFrameDelivered(audio_frame->packet_infos_);

  return muted ? AudioMixer::Source::AudioFrameInfo::kMuted
               : AudioMixer::Source::AudioFrameInfo::kNormal;
}

int AudioReceiveStream::Ssrc() const {
  return config_.rtp.remote_ssrc;
}

int AudioReceiveStream::PreferredSampleRate() const {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  // Return the bigger of playout and receive frequency in the ACM.
  return std::max(acm_receiver_.last_packet_sample_rate_hz().value_or(0),
                  acm_receiver_.last_output_sample_rate_hz());
}

uint32_t AudioReceiveStream::id() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_.rtp.remote_ssrc;
}

absl::optional<Syncable::Info> AudioReceiveStream::GetInfo() const {
  // TODO(bugs.webrtc.org/11993): This is called via RtpStreamsSynchronizer,
  // expect to be called on the network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  Syncable::Info info;
  if (rtp_rtcp_->RemoteNTP(&info.capture_time_ntp_secs,
                           &info.capture_time_ntp_frac,
                           /*rtcp_arrival_time_secs=*/nullptr,
                           /*rtcp_arrival_time_frac=*/nullptr,
                           &info.capture_time_source_clock) != 0) {
    return absl::nullopt;
  }

  if (!last_received_rtp_timestamp_ || !last_received_rtp_system_time_ms_) {
    return absl::nullopt;
  }
  info.latest_received_capture_timestamp = *last_received_rtp_timestamp_;
  info.latest_receive_time_ms = *last_received_rtp_system_time_ms_;

  int jitter_buffer_delay = acm_receiver_.FilteredCurrentDelayMs();
  info.current_delay_ms = jitter_buffer_delay + playout_delay_ms_;

  return info;
}

bool AudioReceiveStream::GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                                                int64_t* time_ms) const {
  // Called on video capture thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playout_timestamp_rtp_time_ms_)
    return false;
  *rtp_timestamp = playout_timestamp_rtp_;
  *time_ms = playout_timestamp_rtp_time_ms_.value();
  return true;
}

void AudioReceiveStream::SetEstimatedPlayoutNtpTimestampMs(
    int64_t ntp_timestamp_ms,
    int64_t time_ms) {
  // Called on video capture thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  playout_timestamp_ntp_ = ntp_timestamp_ms;
  playout_timestamp_ntp_time_ms_ = time_ms;
}

bool AudioReceiveStream::SetMinimumPlayoutDelay(int delay_ms) {
  // TODO(bugs.webrtc.org/11993): This should run on the network thread.
  // We get here via RtpStreamsSynchronizer. Once that's done, many (all?) of
  // these locks aren't needed.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // Limit to range accepted by both VoE and ACM, so we're at least getting as
  // close as possible, instead of failing.
  delay_ms = rtc::SafeClamp(delay_ms, kVoiceEngineMinMinPlayoutDelayMs,
                            kVoiceEngineMaxMinPlayoutDelayMs);
  if (acm_receiver_.SetMinimumDelay(delay_ms) != 0) {
    RTC_DLOG(LS_ERROR)
        << "SetMinimumPlayoutDelay() failed to set min playout delay";
    return false;
  }
  return true;
}

void AudioReceiveStream::AssociateSendStream(AudioSendStream* send_stream) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  associated_send_stream_ = send_stream;
}

void AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(bugs.webrtc.org/11993): Expect to be called exclusively on the
  // network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);

  // Store playout timestamp for the received RTCP packet
  UpdatePlayoutTimestamp(true, rtc::TimeMillis());

  // Deliver RTCP packet to RTP/RTCP module for parsing
  rtp_rtcp_->IncomingRtcpPacket(packet, length);

  int64_t rtt = GetRTT();
  if (rtt == 0) {
    // Waiting for valid RTT.
    return;
  }

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (rtp_rtcp_->RemoteNTP(&ntp_secs, &ntp_frac,
                           /*rtcp_arrival_time_secs=*/nullptr,
                           /*rtcp_arrival_time_frac=*/nullptr,
                           &rtp_timestamp) != 0) {
    // Waiting for RTCP.
    return;
  }

  {
    MutexLock lock(&ts_stats_lock_);
    ntp_estimator_.UpdateRtcpTimestamp(rtt, ntp_secs, ntp_frac, rtp_timestamp);
    absl::optional<int64_t> remote_to_local_clock_offset_ms =
        ntp_estimator_.EstimateRemoteToLocalClockOffsetMs();
    if (remote_to_local_clock_offset_ms.has_value()) {
      capture_clock_offset_updater_.SetRemoteToLocalClockOffset(
          Int64MsToQ32x32(*remote_to_local_clock_offset_ms));
    }
  }
}

void AudioReceiveStream::SetSyncGroup(const std::string& sync_group) {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  config_.sync_group = sync_group;
}

void AudioReceiveStream::SetLocalSsrc(uint32_t local_ssrc) {
  // TODO(bugs.webrtc.org/11993): Expect to be called on the network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // TODO(tommi): Consider storing local_ssrc in one place.
  config_.rtp.local_ssrc = local_ssrc;
  rtp_rtcp_->SetLocalSsrc(local_ssrc);
}

uint32_t AudioReceiveStream::local_ssrc() const {
  // TODO(bugs.webrtc.org/11993): Expect to be called on the network thread.
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK_EQ(config_.rtp.local_ssrc, rtp_rtcp_->local_media_ssrc());
  return config_.rtp.local_ssrc;
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  return config_;
}

const AudioSendStream* AudioReceiveStream::GetAssociatedSendStreamForTesting()
    const {
  RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
  return associated_send_stream_;
}

internal::AudioState* AudioReceiveStream::audio_state() const {
  auto* audio_state = static_cast<internal::AudioState*>(audio_state_.get());
  RTC_DCHECK(audio_state);
  return audio_state;
}

void AudioReceiveStream::OnReceivedPayloadData(
    rtc::ArrayView<const uint8_t> payload,
    const RTPHeader& rtpHeader) {
  if (!playing_) {
    // Avoid inserting into NetEQ when we are not playing. Count the
    // packet as discarded.

    // If we have a source_tracker_, tell it that the frame has been
    // "delivered". Normally, this happens in AudioReceiveStream when audio
    // frames are pulled out, but when playout is muted, nothing is pulling
    // frames. The downside of this approach is that frames delivered this way
    // won't be delayed for playout, and therefore will be unsynchronized with
    // (a) audio delay when playing and (b) any audio/video synchronization. But
    // the alternative is that muting playout also stops the SourceTracker from
    // updating RtpSource information.
    RtpPacketInfos::vector_type packet_vector = {
        RtpPacketInfo(rtpHeader, clock_->CurrentTime())};
    source_tracker_.OnFrameDelivered(RtpPacketInfos(packet_vector));

    return;
  }

  // Push the incoming payload (parsed and ready for decoding) into the ACM
  if (acm_receiver_.InsertPacket(rtpHeader, payload) != 0) {
    RTC_DLOG(LS_ERROR)
        << "AudioReceiveStream::OnReceivedPayloadData() unable to "
           "push data to the ACM";
    return;
  }

  int64_t round_trip_time = 0;
  rtp_rtcp_->RTT(remote_ssrc_, &round_trip_time, NULL, NULL, NULL);

  std::vector<uint16_t> nack_list = acm_receiver_.GetNackList(round_trip_time);
  if (!nack_list.empty()) {
    // Can't use nack_list.data() since it's not supported by all
    // compilers.
    rtp_rtcp_->SendNACK(&(nack_list[0]), static_cast<int>(nack_list.size()));
  }
}

void AudioReceiveStream::InitFrameTransformerDelegate(
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK(frame_transformer);
  RTC_DCHECK(!frame_transformer_delegate_);
  RTC_DCHECK(worker_thread_->IsCurrent());

  // Pass a callback to OnReceivedPayloadData, to be called by
  // the delegate to receive transformed audio.
  ChannelReceiveFrameTransformerDelegate::ReceiveFrameCallback
      receive_audio_callback = [this](rtc::ArrayView<const uint8_t> packet,
                                      const RTPHeader& header) {
        RTC_DCHECK_RUN_ON(&worker_thread_checker_);
        OnReceivedPayloadData(packet, header);
      };
  frame_transformer_delegate_ =
      rtc::make_ref_counted<ChannelReceiveFrameTransformerDelegate>(
          std::move(receive_audio_callback), std::move(frame_transformer),
          worker_thread_);
  frame_transformer_delegate_->Init();
}

void AudioReceiveStream::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  for (const auto& kv : codecs) {
    RTC_DCHECK_GE(kv.second.clockrate_hz, 1000);
    payload_type_frequencies_[kv.first] = kv.second.clockrate_hz;
  }
  acm_receiver_.SetCodecs(codecs);
}

void AudioReceiveStream::OnRtpPacket(const RtpPacketReceived& packet) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // TODO(bugs.webrtc.org/11993): Expect to be called exclusively on the
  // network thread. Once that's done, the same applies to
  // UpdatePlayoutTimestamp and
  int64_t now_ms = rtc::TimeMillis();

  last_received_rtp_timestamp_ = packet.Timestamp();
  last_received_rtp_system_time_ms_ = now_ms;

  // Store playout timestamp for the received RTP packet
  UpdatePlayoutTimestamp(false, now_ms);

  const auto& it = payload_type_frequencies_.find(packet.PayloadType());
  if (it == payload_type_frequencies_.end())
    return;
  // TODO(nisse): Set payload_type_frequency earlier, when packet is parsed.
  RtpPacketReceived packet_copy(packet);
  packet_copy.set_payload_type_frequency(it->second);

  rtp_receive_statistics_->OnRtpPacket(packet_copy);

  RTPHeader header;
  packet_copy.GetHeader(&header);

  // Interpolates absolute capture timestamp RTP header extension.
  header.extension.absolute_capture_time =
      absolute_capture_time_interpolator_.OnReceivePacket(
          AbsoluteCaptureTimeInterpolator::GetSource(header.ssrc,
                                                     header.arrOfCSRCs),
          header.timestamp,
          rtc::saturated_cast<uint32_t>(packet_copy.payload_type_frequency()),
          header.extension.absolute_capture_time);

  ReceivePacket(packet_copy.data(), packet_copy.size(), header);
}

void AudioReceiveStream::ReceivePacket(const uint8_t* packet,
                                       size_t packet_length,
                                       const RTPHeader& header) {
  const uint8_t* payload = packet + header.headerLength;
  RTC_DCHECK_GE(packet_length, header.headerLength);
  size_t payload_length = packet_length - header.headerLength;

  size_t payload_data_length = payload_length - header.paddingLength;

  // E2EE Custom Audio Frame Decryption (This is optional).
  // Keep this buffer around for the lifetime of the OnReceivedPayloadData call.
  rtc::Buffer decrypted_audio_payload;
  if (frame_decryptor_ != nullptr) {
    const size_t max_plaintext_size = frame_decryptor_->GetMaxPlaintextByteSize(
        cricket::MEDIA_TYPE_AUDIO, payload_length);
    decrypted_audio_payload.SetSize(max_plaintext_size);

    const std::vector<uint32_t> csrcs(header.arrOfCSRCs,
                                      header.arrOfCSRCs + header.numCSRCs);
    const FrameDecryptorInterface::Result decrypt_result =
        frame_decryptor_->Decrypt(
            cricket::MEDIA_TYPE_AUDIO, csrcs,
            /*additional_data=*/nullptr,
            rtc::ArrayView<const uint8_t>(payload, payload_data_length),
            decrypted_audio_payload);

    if (decrypt_result.IsOk()) {
      decrypted_audio_payload.SetSize(decrypt_result.bytes_written);
    } else {
      // Interpret failures as a silent frame.
      decrypted_audio_payload.SetSize(0);
    }

    payload = decrypted_audio_payload.data();
    payload_data_length = decrypted_audio_payload.size();
  } else if (crypto_options_.sframe.require_frame_encryption) {
    RTC_DLOG(LS_ERROR)
        << "FrameDecryptor required but not set, dropping packet";
    payload_data_length = 0;
  }

  rtc::ArrayView<const uint8_t> payload_data(payload, payload_data_length);
  if (frame_transformer_delegate_) {
    // Asynchronously transform the received payload. After the payload is
    // transformed, the delegate will call OnReceivedPayloadData to handle it.
    frame_transformer_delegate_->Transform(payload_data, header, remote_ssrc_);
  } else {
    OnReceivedPayloadData(payload_data, header);
  }
}

void AudioReceiveStream::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router);
  RTC_DCHECK(!packet_router_);
  constexpr bool remb_candidate = false;
  packet_router->AddReceiveRtpModule(rtp_rtcp_.get(), remb_candidate);
  packet_router_ = packet_router;
}

void AudioReceiveStream::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  RTC_DCHECK(packet_router_);
  packet_router_->RemoveReceiveRtpModule(rtp_rtcp_.get());
  packet_router_ = nullptr;
}

void AudioReceiveStream::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  // None of these functions can fail.
  if (enable) {
    rtp_receive_statistics_->SetMaxReorderingThreshold(max_packets);
    acm_receiver_.EnableNack(max_packets);
  } else {
    rtp_receive_statistics_->SetMaxReorderingThreshold(
        kDefaultMaxReorderingThreshold);
    acm_receiver_.DisableNack();
  }
}

void AudioReceiveStream::RtcpPacketTypesCounterUpdated(
    uint32_t ssrc,
    const RtcpPacketTypeCounter& packet_counter) {
  if (ssrc != remote_ssrc_) {
    return;
  }
  MutexLock lock(&rtcp_counter_mutex_);
  rtcp_packet_type_counter_ = packet_counter;
}

absl::optional<int64_t>
AudioReceiveStream::GetCurrentEstimatedPlayoutNtpTimestampMs(
    int64_t now_ms) const {
  RTC_DCHECK_RUN_ON(&worker_thread_checker_);
  if (!playout_timestamp_ntp_ || !playout_timestamp_ntp_time_ms_)
    return absl::nullopt;

  int64_t elapsed_ms = now_ms - *playout_timestamp_ntp_time_ms_;
  return *playout_timestamp_ntp_ + elapsed_ms;
}

// RTC_RUN_ON(worker_thread_checker_)
void AudioReceiveStream::UpdatePlayoutTimestamp(bool rtcp, int64_t now_ms) {
  // TODO(bugs.webrtc.org/11993): Expect to be called exclusively on the
  // network thread. Once that's done, we won't need video_sync_lock_.

  jitter_buffer_playout_timestamp_ = acm_receiver_.GetPlayoutTimestamp();

  if (!jitter_buffer_playout_timestamp_) {
    // This can happen if this channel has not received any RTP packets. In
    // this case, NetEq is not capable of computing a playout timestamp.
    return;
  }

  uint16_t delay_ms = 0;
  if (_audioDeviceModulePtr->PlayoutDelay(&delay_ms) == -1) {
    RTC_DLOG(LS_WARNING)
        << "AudioReceiveStream::UpdatePlayoutTimestamp() failed to read"
           " playout delay from the ADM";
    return;
  }

  RTC_DCHECK(jitter_buffer_playout_timestamp_);
  uint32_t playout_timestamp = *jitter_buffer_playout_timestamp_;

  // Remove the playout delay.
  playout_timestamp -= (delay_ms * (GetRtpTimestampRateHz() / 1000));

  if (!rtcp && playout_timestamp != playout_timestamp_rtp_) {
    playout_timestamp_rtp_ = playout_timestamp;
    playout_timestamp_rtp_time_ms_ = now_ms;
  }
  playout_delay_ms_ = delay_ms;
}

int AudioReceiveStream::GetRtpTimestampRateHz() const {
  const auto decoder = acm_receiver_.LastDecoder();
  // Default to the playout frequency if we've not gotten any packets yet.
  // TODO(ossu): Zero clockrate can only happen if we've added an external
  // decoder for a format we don't support internally. Remove once that way of
  // adding decoders is gone!
  // TODO(kwiberg): `decoder->second.clockrate_hz` is an RTP clockrate as it
  // should, but `acm_receiver_.last_output_sample_rate_hz()` is a codec sample
  // rate, which is not always the same thing.
  return (decoder && decoder->second.clockrate_hz != 0)
             ? decoder->second.clockrate_hz
             : acm_receiver_.last_output_sample_rate_hz();
}

int64_t AudioReceiveStream::GetRTT() const {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  std::vector<ReportBlockData> report_blocks =
      rtp_rtcp_->GetLatestReportBlockData();

  if (report_blocks.empty()) {
    // Try fall back on an RTT from an associated channel.
    if (!associated_send_channel_) {
      return 0;
    }
    return associated_send_channel_->GetRTT();
  }

  // TODO(nisse): This method computes RTT based on sender reports, even though
  // a receive stream is not supposed to do that.
  for (const ReportBlockData& data : report_blocks) {
    if (data.report_block().sender_ssrc == remote_ssrc_) {
      return data.last_rtt_ms();
    }
  }
  return 0;
}

}  // namespace internal
}  // namespace webrtc
