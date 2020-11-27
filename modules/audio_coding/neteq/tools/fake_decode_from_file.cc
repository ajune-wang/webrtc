/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/fake_decode_from_file.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace test {

namespace {

class FakeEncodedFrame : public AudioDecoder::EncodedAudioFrame {
 public:
  FakeEncodedFrame(AudioDecoder* decoder, rtc::Buffer&& payload)
      : decoder_(decoder), payload_(std::move(payload)) {}

  size_t Duration() const override {
    const int ret = decoder_->PacketDuration(payload_.data(), payload_.size());
    return ret < 0 ? 0 : static_cast<size_t>(ret);
  }

  absl::optional<DecodeResult> Decode(
      rtc::ArrayView<int16_t> decoded) const override {
#if 0
    auto speech_type = AudioDecoder::kSpeech;
    const int ret = decoder_->Decode(
        payload_.data(), payload_.size(), decoder_->SampleRateHz(),
        decoded.size() * sizeof(int16_t), decoded.data(), &speech_type);
    return ret < 0 ? absl::nullopt
                   : absl::optional<DecodeResult>(
                         {static_cast<size_t>(ret), speech_type});
#endif
    return absl::nullopt;
  }

  // This is to mimic OpusFrame.
  bool IsDtxPacket() const override {
    uint32_t original_payload_size_bytes =
        ByteReader<uint32_t>::ReadLittleEndian(&payload_.data()[8]);
    return original_payload_size_bytes <= 2;
  }

 private:
  AudioDecoder* const decoder_;
  const rtc::Buffer payload_;
};

}  // namespace

std::vector<AudioDecoder::ParseResult> FakeDecodeFromFile::ParsePayload(
    rtc::Buffer&& payload,
    uint32_t timestamp) {
  (void)cng_mode_;
  std::vector<ParseResult> results;
  std::unique_ptr<EncodedAudioFrame> frame(
      new FakeEncodedFrame(this, std::move(payload)));
  results.emplace_back(timestamp, 0, std::move(frame));
  return results;
}

int FakeDecodeFromFile::PacketDuration(const uint8_t* encoded,
                                       size_t encoded_len) const {
  const uint32_t original_payload_size_bytes =
      encoded_len < 8 + sizeof(uint32_t)
          ? 0
          : ByteReader<uint32_t>::ReadLittleEndian(&encoded[8]);
  const uint32_t samples_to_decode =
      encoded_len < 4 + sizeof(uint32_t)
          ? 0
          : ByteReader<uint32_t>::ReadLittleEndian(&encoded[4]);
  if (  // Decoder is asked to produce codec-internal comfort noise
      encoded_len == 0 ||
      // Comfort noise payload
      original_payload_size_bytes <= 2 || samples_to_decode == 0 ||
      // Erroneous duration since it is not a multiple of 10ms
      samples_to_decode % rtc::CheckedDivExact(SampleRateHz(), 100) != 0) {
    if (last_decoded_length_ > 0) {
      // Use length of last decoded packet.
      return rtc::dchecked_cast<int>(last_decoded_length_);
    } else {
      // This is the first packet to decode, and we do not know the length of
      // it. Set it to 10 ms.
      return rtc::CheckedDivExact(SampleRateHz(), 100);
    }
  }
  return samples_to_decode;
}

void FakeDecodeFromFile::PrepareEncoded(uint32_t timestamp,
                                        size_t samples,
                                        size_t original_payload_size_bytes,
                                        rtc::ArrayView<uint8_t> encoded) {
  RTC_CHECK_GE(encoded.size(), 12);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[0], timestamp);
  ByteWriter<uint32_t>::WriteLittleEndian(&encoded[4],
                                          rtc::checked_cast<uint32_t>(samples));
  ByteWriter<uint32_t>::WriteLittleEndian(
      &encoded[8], rtc::checked_cast<uint32_t>(original_payload_size_bytes));
}

}  // namespace test
}  // namespace webrtc
