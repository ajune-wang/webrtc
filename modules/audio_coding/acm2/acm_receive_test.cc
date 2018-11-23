/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_receive_test.h"

#include <stdio.h>

#include <memory>

#include "absl/strings/match.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "modules/audio_coding/neteq/tools/packet_source.h"
#include "modules/include/module_common_types.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
// Returns true if the codec should be registered, otherwise false.
bool ShouldUseThisCodec(const SdpAudioFormat& format) {
  if (absl::EqualsIgnoreCase(format.name, "CN") &&
      format.clockrate_hz == 48000)
    return false;  // Skip 48 kHz comfort noise.

  if (absl::EqualsIgnoreCase(format.name, "telephone-event"))
    return false;  // Skip DTFM.

  return true;
}

// Remaps payload types from ACM's default to those used in the resource file
// neteq_universal_new.rtp. Returns true if the codec should be registered,
// otherwise false. The payload types are set as follows (all are mono codecs):
// PCMu = 0;
// PCMa = 8;
// Comfort noise 8 kHz = 13
// Comfort noise 16 kHz = 98
// Comfort noise 32 kHz = 99
// iLBC = 102
// iSAC wideband = 103
// iSAC super-wideband = 104
// AVT/DTMF = 106
// RED = 117
// PCM16b 8 kHz = 93
// PCM16b 16 kHz = 94
// PCM16b 32 kHz = 95
// G.722 = 94
absl::optional<int> GetPayloadTypeMapping(const SdpAudioFormat& format) {
  if (format.num_channels != 1)
    return absl::nullopt;  // Don't use non-mono codecs.

  // Re-map pltypes to those used in the NetEq test files.
  int payload_type = -1;
  if (absl::EqualsIgnoreCase(format.name, "PCMU") &&
             format.clockrate_hz == 8000) {
    payload_type = 0;
  } else if (absl::EqualsIgnoreCase(format.name, "PCMA") &&
             format.clockrate_hz == 8000) {
    payload_type = 8;
  } else if (absl::EqualsIgnoreCase(format.name, "CN") &&
             format.clockrate_hz == 8000) {
    payload_type = 13;
  } else if (absl::EqualsIgnoreCase(format.name, "CN") &&
             format.clockrate_hz == 16000) {
    payload_type = 98;
  } else if (absl::EqualsIgnoreCase(format.name, "CN") &&
             format.clockrate_hz == 32000) {
    payload_type = 99;
  } else if (absl::EqualsIgnoreCase(format.name, "ILBC")) {
    payload_type = 102;
  } else if (absl::EqualsIgnoreCase(format.name, "ISAC") &&
             format.clockrate_hz == 16000) {
    payload_type = 103;
  } else if (absl::EqualsIgnoreCase(format.name, "ISAC") &&
             format.clockrate_hz == 32000) {
    payload_type = 104;
  } else if (absl::EqualsIgnoreCase(format.name, "telephone-event") &&
             format.clockrate_hz == 8000) {
    payload_type = 106;
  } else if (absl::EqualsIgnoreCase(format.name, "telephone-event") &&
             format.clockrate_hz == 16000) {
    payload_type = 114;
  } else if (absl::EqualsIgnoreCase(format.name, "telephone-event") &&
             format.clockrate_hz == 32000) {
    payload_type = 115;
  } else if (absl::EqualsIgnoreCase(format.name, "telephone-event") &&
             format.clockrate_hz == 48000) {
    payload_type = 116;
  } else if (absl::EqualsIgnoreCase(format.name, "red")) {
    payload_type = 117;
  } else if (absl::EqualsIgnoreCase(format.name, "L16") &&
             format.clockrate_hz == 8000) {
    payload_type = 93;
  } else if (absl::EqualsIgnoreCase(format.name, "L16") &&
             format.clockrate_hz == 16000) {
    payload_type = 94;
  } else if (absl::EqualsIgnoreCase(format.name, "L16") &&
             format.clockrate_hz == 32000) {
    payload_type = 95;
  } else if (absl::EqualsIgnoreCase(format.name, "G722")) {
    payload_type = 9;
  } else {
    // Don't use any other codecs.
    return absl::nullopt;
  }
  return payload_type;
}

AudioCodingModule::Config MakeAcmConfig(
    Clock* clock,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory) {
  AudioCodingModule::Config config;
  config.clock = clock;
  config.decoder_factory = std::move(decoder_factory);
  return config;
}

}  // namespace

AcmReceiveTestOldApi::AcmReceiveTestOldApi(
    PacketSource* packet_source,
    AudioSink* audio_sink,
    int output_freq_hz,
    NumOutputChannels exptected_output_channels,
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory)
    : clock_(0),
      decoder_factory_(decoder_factory),
      acm_(webrtc::AudioCodingModule::Create(
          MakeAcmConfig(&clock_, std::move(decoder_factory)))),
      packet_source_(packet_source),
      audio_sink_(audio_sink),
      output_freq_hz_(output_freq_hz),
      exptected_output_channels_(exptected_output_channels) {}

AcmReceiveTestOldApi::~AcmReceiveTestOldApi() = default;

void AcmReceiveTestOldApi::RegisterDefaultCodecs() {
  std::map<int, SdpAudioFormat> receive_codecs;
  int payload_type = 1;
  for (const auto& spec : decoder_factory_->GetSupportedDecoders()) {
    if (ShouldUseThisCodec(spec.format)) {
      receive_codecs.emplace(std::make_pair(payload_type, spec.format));
    }
  }
  acm_->SetReceiveCodecs(receive_codecs);
}

void AcmReceiveTestOldApi::RegisterNetEqTestCodecs() {
  std::map<int, SdpAudioFormat> receive_codecs;
  for (const auto& spec : decoder_factory_->GetSupportedDecoders()) {
    if (ShouldUseThisCodec(spec.format)) {
      auto payload_type = GetPayloadTypeMapping(spec.format);
      if (payload_type) {
        receive_codecs.emplace(std::make_pair(*payload_type, spec.format));
      }
    }
  }
  acm_->SetReceiveCodecs(receive_codecs);
}

void AcmReceiveTestOldApi::Run() {
  for (std::unique_ptr<Packet> packet(packet_source_->NextPacket()); packet;
       packet = packet_source_->NextPacket()) {
    // Pull audio until time to insert packet.
    while (clock_.TimeInMilliseconds() < packet->time_ms()) {
      AudioFrame output_frame;
      bool muted;
      EXPECT_EQ(0,
                acm_->PlayoutData10Ms(output_freq_hz_, &output_frame, &muted));
      ASSERT_EQ(output_freq_hz_, output_frame.sample_rate_hz_);
      ASSERT_FALSE(muted);
      const size_t samples_per_block =
          static_cast<size_t>(output_freq_hz_ * 10 / 1000);
      EXPECT_EQ(samples_per_block, output_frame.samples_per_channel_);
      if (exptected_output_channels_ != kArbitraryChannels) {
        if (output_frame.speech_type_ == webrtc::AudioFrame::kPLC) {
          // Don't check number of channels for PLC output, since each test run
          // usually starts with a short period of mono PLC before decoding the
          // first packet.
        } else {
          EXPECT_EQ(exptected_output_channels_, output_frame.num_channels_);
        }
      }
      ASSERT_TRUE(audio_sink_->WriteAudioFrame(output_frame));
      clock_.AdvanceTimeMilliseconds(10);
      AfterGetAudio();
    }

    // Insert packet after converting from RTPHeader to WebRtcRTPHeader.
    WebRtcRTPHeader header;
    header.header = packet->header();
    header.frameType = kAudioFrameSpeech;
    EXPECT_EQ(0,
              acm_->IncomingPacket(
                  packet->payload(),
                  static_cast<int32_t>(packet->payload_length_bytes()), header))
        << "Failure when inserting packet:" << std::endl
        << "  PT = " << static_cast<int>(header.header.payloadType) << std::endl
        << "  TS = " << header.header.timestamp << std::endl
        << "  SN = " << header.header.sequenceNumber;
  }
}

AcmReceiveTestToggleOutputFreqOldApi::AcmReceiveTestToggleOutputFreqOldApi(
    PacketSource* packet_source,
    AudioSink* audio_sink,
    int output_freq_hz_1,
    int output_freq_hz_2,
    int toggle_period_ms,
    NumOutputChannels exptected_output_channels)
    : AcmReceiveTestOldApi(packet_source,
                           audio_sink,
                           output_freq_hz_1,
                           exptected_output_channels,
                           CreateBuiltinAudioDecoderFactory()),
      output_freq_hz_1_(output_freq_hz_1),
      output_freq_hz_2_(output_freq_hz_2),
      toggle_period_ms_(toggle_period_ms),
      last_toggle_time_ms_(clock_.TimeInMilliseconds()) {}

void AcmReceiveTestToggleOutputFreqOldApi::AfterGetAudio() {
  if (clock_.TimeInMilliseconds() >= last_toggle_time_ms_ + toggle_period_ms_) {
    output_freq_hz_ = (output_freq_hz_ == output_freq_hz_1_)
                          ? output_freq_hz_2_
                          : output_freq_hz_1_;
    last_toggle_time_ms_ = clock_.TimeInMilliseconds();
  }
}

}  // namespace test
}  // namespace webrtc
