/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_test.h"

#include <iomanip>
#include <iostream>

#include "modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
namespace test {
namespace {

absl::optional<Operations> ActionToOperations(
    absl::optional<NetEqSimulator::Action> a) {
  if (!a) {
    return absl::nullopt;
  }
  switch (*a) {
    case NetEqSimulator::Action::kAccelerate:
      return absl::make_optional(kAccelerate);
    case NetEqSimulator::Action::kExpand:
      return absl::make_optional(kExpand);
    case NetEqSimulator::Action::kNormal:
      return absl::make_optional(kNormal);
    case NetEqSimulator::Action::kPreemptiveExpand:
      return absl::make_optional(kPreemptiveExpand);
  }
}

}  // namespace

void DefaultNetEqTestErrorCallback::OnInsertPacketError(
    const NetEqInput::PacketData& packet) {
  std::cerr << "InsertPacket returned an error." << std::endl;
  std::cerr << "Packet data: " << packet.ToString() << std::endl;
  FATAL();
}

void DefaultNetEqTestErrorCallback::OnGetAudioError() {
  std::cerr << "GetAudio returned an error." << std::endl;
  FATAL();
}

NetEqTest::NetEqTest(const NetEq::Config& config,
                     rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
                     const DecoderMap& codecs,
                     std::unique_ptr<std::ofstream> text_log,
                     std::unique_ptr<NetEqInput> input,
                     std::unique_ptr<AudioSink> output,
                     Callbacks callbacks)
    : neteq_(NetEq::Create(config, decoder_factory)),
      input_(std::move(input)),
      output_(std::move(output)),
      callbacks_(callbacks),
      sample_rate_hz_(config.sample_rate_hz),
      text_log_(std::move(text_log)) {
  RTC_CHECK(!config.enable_muted_state)
      << "The code does not handle enable_muted_state";
  RegisterDecoders(codecs);
}

NetEqTest::~NetEqTest() = default;

int64_t NetEqTest::Run() {
  const auto start_state = GetNetEqState();

  while (RunToNextGetAudio()) {
  }

  const auto end_state = GetNetEqState();
  const int64_t simulation_time_ms = (end_state.current_simulation_time_us -
                                      start_state.current_simulation_time_us) /
                                     1000;
  if (callbacks_.simulation_ended_callback) {
    callbacks_.simulation_ended_callback->SimulationEnded(simulation_time_ms);
  }
  return simulation_time_ms;
}

bool NetEqTest::RunToNextGetAudio() {
  const int64_t start_time_us = *input_->NextEventTime();
  int64_t time_now_us = start_time_us;
  current_state_.arrived_packets.clear();

  while (!input_->ended()) {
    // Advance time to next event.
    RTC_DCHECK(input_->NextEventTime());
    time_now_us = *input_->NextEventTime();
    // Check if it is time to insert packet.
    if (input_->NextPacketTime() && time_now_us >= *input_->NextPacketTime()) {
      std::unique_ptr<NetEqInput::PacketData> packet_data = input_->PopPacket();
      RTC_CHECK(packet_data);
      const size_t payload_data_length =
          packet_data->payload.size() - packet_data->header.paddingLength;
      if (payload_data_length != 0) {
        int error = neteq_->InsertPacket(
            packet_data->header,
            rtc::ArrayView<const uint8_t>(packet_data->payload),
            static_cast<uint32_t>(packet_data->time_us * sample_rate_hz_ /
                                  1000000));
        if (error != NetEq::kOK && callbacks_.error_callback) {
          callbacks_.error_callback->OnInsertPacketError(*packet_data);
        }
        if (callbacks_.post_insert_packet) {
          callbacks_.post_insert_packet->AfterInsertPacket(*packet_data,
                                                           neteq_.get());
        }
      } else {
        neteq_->InsertEmptyPacket(packet_data->header);
      }
      NetEqOperationsAndState ops_state = neteq_->GetOperationsAndState();
      NetEqSimulator::RtpPacketInfo arrived_packet;
      arrived_packet.arrival_time_us = time_now_us;
      arrived_packet.audio_content_samples =
          ops_state.last_audio_content_samples;
      arrived_packet.dtx_packet = ops_state.last_dtx_packet;
      arrived_packet.padding = ops_state.last_received_padding;
      arrived_packet.rtp_timestamp = ops_state.last_rtp_timestamp;
      arrived_packet.sequence_number = ops_state.last_received_sequence_number;
      current_state_.arrived_packets.push_back(arrived_packet);
      if (text_log_) {
        const auto ops_state = neteq_->GetOperationsAndState();
        const auto delta_wallclock =
            last_packet_time_us_ ? (time_now_us - *last_packet_time_us_) : -1;
        const auto delta_timestamp =
            last_packet_timestamp_
                ? (static_cast<int64_t>(packet_data->header.timestamp) -
                   *last_packet_timestamp_) *
                      1000 / sample_rate_hz_
                : -1;
        const auto packet_size_bytes =
            packet_data->payload.size() == 12
                ? ByteReader<uint32_t>::ReadLittleEndian(
                      &packet_data->payload[8])
                : -1;
        *text_log_ << "Packet   - wallclock: " << std::setw(5) << time_now_us
                   << ", delta wc: " << std::setw(4) << delta_wallclock
                   << ", seq_no: " << packet_data->header.sequenceNumber
                   << ", timestamp: " << std::setw(10)
                   << packet_data->header.timestamp
                   << ", delta ts: " << std::setw(4) << delta_timestamp
                   << ", size: " << std::setw(5) << packet_size_bytes
                   << ", frame size: " << std::setw(3)
                   << ops_state.current_frame_size_ms
                   << ", buffer size: " << std::setw(4)
                   << ops_state.current_buffer_size_ms << std::endl;
      }
      last_packet_time_us_ = absl::make_optional<int>(time_now_us);
      last_packet_timestamp_ =
          absl::make_optional<uint32_t>(packet_data->header.timestamp);
    }

    // Check if it is time to get output audio.
    if (input_->NextOutputEventTime() &&
        time_now_us >= *input_->NextOutputEventTime()) {
      if (callbacks_.get_audio_callback) {
        callbacks_.get_audio_callback->BeforeGetAudio(neteq_.get());
      }
      AudioFrame out_frame;
      bool muted;
      int error = neteq_->GetAudio(&out_frame, &muted,
                                   ActionToOperations(next_action_));
      next_action_ = absl::nullopt;
      RTC_CHECK(!muted) << "The code does not handle enable_muted_state";
      if (error != NetEq::kOK) {
        if (callbacks_.error_callback) {
          callbacks_.error_callback->OnGetAudioError();
        }
      } else {
        sample_rate_hz_ = out_frame.sample_rate_hz_;
      }
      if (callbacks_.get_audio_callback) {
        callbacks_.get_audio_callback->AfterGetAudio(time_now_us, out_frame,
                                                     muted, neteq_.get());
      }

      if (output_) {
        RTC_CHECK(output_->WriteArray(
            out_frame.data(),
            out_frame.samples_per_channel_ * out_frame.num_channels_));
      }

      input_->AdvanceOutputEvent();
      const auto operations_state = neteq_->GetOperationsAndState();
      const auto lifetime_stats = LifetimeStats();
      current_state_.sample_rate_hz = operations_state.sample_rate_hz;
      current_state_.last_decoded_timestamp =
          operations_state.last_decoded_timestamp;
      current_state_.current_delay_ms = operations_state.current_buffer_size_ms;
      current_state_.packet_buffer_flushed =
          lifetime_stats.packet_buffer_flushes >
          prev_lifetime_stats_.packet_buffer_flushes;
      current_state_.current_simulation_time_us = time_now_us;
      // The current buffer size in samples.
      current_state_.buffer_size_samples = current_state_.current_delay_ms *
                                           current_state_.sample_rate_hz / 1000;
      // The audio sample rate in hertz.
      current_state_.total_playout_samples =
          lifetime_stats.total_samples_received;
      current_state_.total_discarded_samples = lifetime_stats.discarded_samples;
      current_state_.total_concealed_samples = lifetime_stats.concealed_samples;
      current_state_.total_concealed_nonsilent_samples =
          lifetime_stats.voice_concealed_samples;
      current_state_.total_accelerated_samples =
          lifetime_stats.accelerated_samples;
      current_state_.total_decelerated_samples =
          lifetime_stats.preemptive_samples;

      if (text_log_) {
        const bool plc =
            (out_frame.speech_type_ == AudioFrame::SpeechType::kPLC) ||
            (out_frame.speech_type_ == AudioFrame::SpeechType::kPLCCNG);
        const bool cng = out_frame.speech_type_ == AudioFrame::SpeechType::kCNG;
        const bool voice_concealed =
            lifetime_stats.voice_concealed_samples >
            prev_lifetime_stats_.voice_concealed_samples;
        *text_log_ << "GetAudio - wallclock: " << std::setw(5) << time_now_us
                   << ", delta wc: " << std::setw(4)
                   << (input_->NextEventTime().value_or(time_now_us) -
                       start_time_us)
                   << ", CNG: " << cng << ", PLC: " << plc
                   << ", voice concealed: " << voice_concealed
                   << ", buffer size: " << std::setw(4)
                   << current_state_.current_delay_ms << std::endl;
        if (operations_state.discarded_primary_packets >
            prev_ops_state_.discarded_primary_packets) {
          *text_log_ << "Discarded "
                     << (operations_state.discarded_primary_packets -
                         prev_ops_state_.discarded_primary_packets)
                     << " primary packets." << std::endl;
        }
        if (lifetime_stats.packet_buffer_flushes >
            prev_lifetime_stats_.packet_buffer_flushes) {
          *text_log_ << "Flushed packet buffer "
                     << (lifetime_stats.packet_buffer_flushes -
                         prev_lifetime_stats_.packet_buffer_flushes)
                     << " times." << std::endl;
        }
      }
      prev_lifetime_stats_ = lifetime_stats;
      prev_ops_state_ = operations_state;
      return !input_->ended();
    }
  }
  return false;
}

void NetEqTest::SetNextAction(NetEqTest::Action next_operation) {
  next_action_ = absl::optional<Action>(next_operation);
}

NetEqTest::NetEqState NetEqTest::GetNetEqState() {
  return current_state_;
}

NetEqNetworkStatistics NetEqTest::SimulationStats() {
  NetEqNetworkStatistics stats;
  RTC_CHECK_EQ(neteq_->NetworkStatistics(&stats), 0);
  return stats;
}

NetEqLifetimeStatistics NetEqTest::LifetimeStats() const {
  return neteq_->GetLifetimeStatistics();
}

NetEqTest::DecoderMap NetEqTest::StandardDecoderMap() {
  DecoderMap codecs = {
    {0, SdpAudioFormat("pcmu", 8000, 1)},
    {8, SdpAudioFormat("pcma", 8000, 1)},
#ifdef WEBRTC_CODEC_ILBC
    {102, SdpAudioFormat("ilbc", 8000, 1)},
#endif
    {103, SdpAudioFormat("isac", 16000, 1)},
#if !defined(WEBRTC_ANDROID)
    {104, SdpAudioFormat("isac", 32000, 1)},
#endif
#ifdef WEBRTC_CODEC_OPUS
    {111, SdpAudioFormat("opus", 48000, 2)},
#endif
    {93, SdpAudioFormat("l16", 8000, 1)},
    {94, SdpAudioFormat("l16", 16000, 1)},
    {95, SdpAudioFormat("l16", 32000, 1)},
    {96, SdpAudioFormat("l16", 48000, 1)},
    {9, SdpAudioFormat("g722", 8000, 1)},
    {106, SdpAudioFormat("telephone-event", 8000, 1)},
    {114, SdpAudioFormat("telephone-event", 16000, 1)},
    {115, SdpAudioFormat("telephone-event", 32000, 1)},
    {116, SdpAudioFormat("telephone-event", 48000, 1)},
    {117, SdpAudioFormat("red", 8000, 1)},
    {13, SdpAudioFormat("cn", 8000, 1)},
    {98, SdpAudioFormat("cn", 16000, 1)},
    {99, SdpAudioFormat("cn", 32000, 1)},
    {100, SdpAudioFormat("cn", 48000, 1)}
  };
  return codecs;
}

void NetEqTest::RegisterDecoders(const DecoderMap& codecs) {
  for (const auto& c : codecs) {
    RTC_CHECK(neteq_->RegisterPayloadType(c.first, c.second))
        << "Cannot register " << c.second.name << " to payload type "
        << c.first;
  }
}

}  // namespace test
}  // namespace webrtc
