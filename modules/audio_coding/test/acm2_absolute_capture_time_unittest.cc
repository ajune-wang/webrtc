/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;

constexpr int kSampleRateHz = 48000;
constexpr int kNumChannels = 2;
constexpr int kFrameSize = kSampleRateHz / 100;
constexpr int kPTimeMs = 20;

class AudioPacketizationCallbackMock : public AudioPacketizationCallback {
 public:
  MOCK_METHOD(int32_t,
              SendData,
              (AudioFrameType frame_type,
               uint8_t payload_type,
               uint32_t timestamp,
               const uint8_t* payload_data,
               size_t payload_len_bytes,
               int64_t absolute_capture_timestamp_ms),
              (override));
};

class AcmAbsoluteCaptureTimestamp : public ::testing::Test {
 public:
  AcmAbsoluteCaptureTimestamp() : audio_frame_(kSampleRateHz, kNumChannels) {}

 protected:
  void SetUp() {
    rtc::scoped_refptr<AudioEncoderFactory> codec_factory =
        CreateBuiltinAudioEncoderFactory();
    acm_ = AudioCodingModule::Create();
    std::unique_ptr<AudioEncoder> encoder = codec_factory->MakeAudioEncoder(
        111, SdpAudioFormat("OPUS", kSampleRateHz, kNumChannels),
        absl::nullopt);
    encoder->SetDtx(true);
    encoder->SetReceiverFrameLengthRange(kPTimeMs, kPTimeMs);
    acm_->SetEncoder(std::move(encoder));
    acm_->RegisterTransportCallback(&transport_);
    for (size_t k = 0; k < audio_.size(); ++k) {
      audio_[k] = 10 * k;
    }
  }

  const AudioFrame& GetAudioWithAbsoluteCaptureTimestamp(
      int64_t absolute_capture_timestamp_ms) {
    audio_frame_.ResetWithoutMuting();
    audio_frame_.UpdateFrame(timestamp_, audio_.data(), kFrameSize,
                             kSampleRateHz,
                             AudioFrame::SpeechType::kNormalSpeech,
                             AudioFrame::VADActivity::kVadActive, kNumChannels);
    audio_frame_.set_absolute_capture_timestamp_ms(
        absolute_capture_timestamp_ms);
    timestamp_ += kFrameSize;
    return audio_frame_;
  }

  std::unique_ptr<AudioCodingModule> acm_;
  AudioPacketizationCallbackMock transport_;
  AudioFrame audio_frame_;
  std::array<int16_t, kFrameSize * kNumChannels> audio_;
  uint32_t timestamp_;
};

TEST_F(AcmAbsoluteCaptureTimestamp, HaveBeginningOfFrameCaptureTime) {
  constexpr int64_t first_absolute_capture_timestamp_ms = 123456789;

  int64_t absolute_capture_timestamp_ms = first_absolute_capture_timestamp_ms;
  EXPECT_CALL(transport_,
              SendData(_, _, _, _, _, first_absolute_capture_timestamp_ms))
      .Times(1);
  EXPECT_CALL(
      transport_,
      SendData(_, _, _, _, _, first_absolute_capture_timestamp_ms + kPTimeMs))
      .Times(1);
  for (int k = 0; k < 5; ++k) {
    acm_->Add10MsData(
        GetAudioWithAbsoluteCaptureTimestamp(absolute_capture_timestamp_ms));
    absolute_capture_timestamp_ms += 10;
  }
}

}  // namespace
}  // namespace webrtc
