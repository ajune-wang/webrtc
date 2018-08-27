/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Test to verify correct operation when using the decoder-internal PLC.

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "modules/audio_coding/neteq/tools/encode_neteq_input.h"
#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {
namespace {

// This class implements a fake decoder. The decoder will read audio from a file
// and present as output, both for regular decoding and for PLC.
class AudioDecoderPlc : public AudioDecoder {
 public:
  AudioDecoderPlc(std::unique_ptr<InputAudioFile> input, int sample_rate_hz)
      : input_(std::move(input)), sample_rate_hz_(sample_rate_hz) {}

  void Reset() override {}
  int SampleRateHz() const override { return sample_rate_hz_; }
  size_t Channels() const override { return 1; }
  int DecodeInternal(const uint8_t* /*encoded*/,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override {
    RTC_CHECK_EQ(encoded_len / 2, 20 * sample_rate_hz_ / 1000);
    RTC_CHECK_EQ(sample_rate_hz, sample_rate_hz_);
    RTC_CHECK(decoded);
    RTC_CHECK(speech_type);
    RTC_CHECK(input_->Read(encoded_len / 2, decoded));
    *speech_type = kSpeech;
    last_was_plc_ = false;
    return encoded_len / 2;
  }

  void GeneratePlc(rtc::BufferT<int16_t>* concealment_audio) override {
    // Must keep a local copy of this since DecodeInternal sets it to false.
    const bool last_was_plc = last_was_plc_;
    SpeechType speech_type;
    std::vector<int16_t> decoded(5760);
    int dec_len = DecodeInternal(nullptr, 2 * 20 * sample_rate_hz_ / 1000,
                                 sample_rate_hz_, decoded.data(), &speech_type);
    concealment_audio->AppendData(decoded.data(), dec_len);
    concealed_samples_ += static_cast<size_t>(dec_len);
    if (!last_was_plc) {
      ++concealment_events_;
    }
    last_was_plc_ = true;
  }

  size_t concealed_samples() { return concealed_samples_; }
  size_t concealment_events() { return concealment_events_; }

 private:
  std::unique_ptr<InputAudioFile> input_;
  int sample_rate_hz_;
  size_t concealed_samples_ = 0;
  size_t concealment_events_ = 0;
  bool last_was_plc_ = false;
};

// An input sample generator which generates only zero-samples.
class ZeroSampleGenerator : public EncodeNetEqInput::Generator {
 public:
  rtc::ArrayView<const int16_t> Generate(size_t num_samples) override {
    vec.resize(num_samples, 0);
    rtc::ArrayView<const int16_t> view(vec);
    RTC_DCHECK_EQ(view.size(), num_samples);
    return view;
  }

 private:
  std::vector<int16_t> vec;
};

// A NetEqInput which connects to another NetEqInput, but drops a number of
// packets on the way.
class LossyInput : public NetEqInput {
 public:
  LossyInput(int loss_cadence, std::unique_ptr<NetEqInput> input)
      : loss_cadence_(loss_cadence), input_(std::move(input)) {}

  absl::optional<int64_t> NextPacketTime() const {
    return input_->NextPacketTime();
  }

  absl::optional<int64_t> NextOutputEventTime() const {
    return input_->NextOutputEventTime();
  }

  std::unique_ptr<PacketData> PopPacket() {
    if (loss_cadence_ != 0 && (++count_ % loss_cadence_) == 0) {
      // Pop one extra packet to create the loss.
      input_->PopPacket();
    }
    return input_->PopPacket();
  }

  void AdvanceOutputEvent() { return input_->AdvanceOutputEvent(); }

  bool ended() const { return input_->ended(); }

  absl::optional<RTPHeader> NextHeader() const { return input_->NextHeader(); }

 private:
  int loss_cadence_;
  int count_ = 0;
  std::unique_ptr<NetEqInput> input_;
};

class AudioChecksumWithOutput : public AudioChecksum {
 public:
  explicit AudioChecksumWithOutput(std::string* output_str)
      : output_str_(*output_str) {}
  ~AudioChecksumWithOutput() { output_str_ = Finish(); }

 private:
  std::string& output_str_;
};

NetEqNetworkStatistics RunTest(int loss_cadence, std::string* checksum) {
  NetEq::Config config;
  config.for_test_no_time_stretching = true;

  // The input is mostly useless. It sends zero-samples to a PCM16b encoder,
  // but the actual encoded samples will never be used by the decoder in the
  // test. See below about the decoder.
  std::unique_ptr<ZeroSampleGenerator> generator(new ZeroSampleGenerator);
  constexpr int kSampleRateHz = 32000;
  constexpr int kPayloadType = 100;
  AudioEncoderPcm16B::Config encoder_config;
  encoder_config.sample_rate_hz = kSampleRateHz;
  encoder_config.payload_type = kPayloadType;
  std::unique_ptr<AudioEncoderPcm16B> encoder(
      new AudioEncoderPcm16B(encoder_config));
  constexpr int kRunTimeMs = 10000;
  std::unique_ptr<EncodeNetEqInput> input(new EncodeNetEqInput(
      std::move(generator), std::move(encoder), kRunTimeMs));
  // Wrap the input in a loss function.
  std::unique_ptr<LossyInput> lossy_input(
      new LossyInput(loss_cadence, std::move(input)));

  // Settinng up decoders.
  NetEqTest::DecoderMap decoders;
  // Using a fake decoder which simply reads the output audio from a file.
  std::unique_ptr<InputAudioFile> input_file(new InputAudioFile(
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm")));
  AudioDecoderPlc dec(std::move(input_file), kSampleRateHz);
  // Masquerading as a PCM16b decoder.
  NetEqTest::ExternalDecoderInfo dec_info = {
      &dec, NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16b_PLC"};
  NetEqTest::ExtDecoderMap external_decoders;
  external_decoders.insert(std::make_pair(kPayloadType, dec_info));

  // Output is simply a checksum calculator.
  std::unique_ptr<AudioChecksum> output(new AudioChecksumWithOutput(checksum));

  // No callback objects.
  NetEqTest::Callbacks callbacks;

  NetEqTest neteq_test(config, decoders, external_decoders,
                       std::move(lossy_input), std::move(output), callbacks);
  EXPECT_LE(kRunTimeMs, neteq_test.Run());

  auto lifetime_stats = neteq_test.LifetimeStats();
  EXPECT_EQ(dec.concealed_samples(), lifetime_stats.concealed_samples);
  EXPECT_EQ(dec.concealment_events(), lifetime_stats.concealment_events);

  return neteq_test.SimulationStats();
}
}  // namespace

TEST(NetEqDecoderPlc, Test) {
  std::string checksum;
  auto stats = RunTest(10, &checksum);

  std::string checksum_no_loss;
  auto stats_no_loss = RunTest(0, &checksum_no_loss);

  EXPECT_EQ(checksum, checksum_no_loss);

  EXPECT_EQ(stats.preemptive_rate, stats_no_loss.preemptive_rate);
  EXPECT_EQ(stats.accelerate_rate, stats_no_loss.accelerate_rate);
  EXPECT_EQ(stats.max_waiting_time_ms, stats_no_loss.max_waiting_time_ms);
}

}  // namespace test
}  // namespace webrtc
