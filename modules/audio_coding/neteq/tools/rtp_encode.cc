/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#endif
#ifdef WEBRTC_LINUX
#include <netinet/in.h>
#endif

#include <iostream>
#include <map>
#include <string>

#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#include "modules/audio_coding/codecs/g722/audio_encoder_g722.h"
#include "modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"
#include "modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"
#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#include "modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/safe_conversions.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
namespace test {
namespace {

// Define command line flags.
DEFINE_bool(list_codecs, false, "Enumerate all codecs");
DEFINE_string(codec, "opus", "Codec to use");
DEFINE_int(frame_len, 0, "Frame length in ms; 0 indicates codec default value");
DEFINE_int(bitrate, 0, "Bitrate in kbps; 0 indicates codec default value");
DEFINE_int(payload_type,
           -1,
           "RTP payload type; -1 indicates codec default value");
DEFINE_int(cng_payload_type,
           -1,
           "RTP payload type for CNG; -1 indicates default value");
DEFINE_int(ssrc, 0, "SSRC to write to the RTP header");
DEFINE_bool(dtx, false, "Use DTX/CNG");
DEFINE_int(sample_rate, 48000, "Sample rate of the input file");
DEFINE_bool(help, false, "Print this message");

// Add new codecs here, and to the map below.
enum class CodecType {
  kOpus,
  kPcmU,
  kPcmA,
  kG722,
  kPcm16b8,
  kPcm16b16,
  kPcm16b32,
  kPcm16b48,
  kIlbc,
  kIsac
};

struct CodecTypeAndDtxType {
  CodecType type;
  bool internal_dtx;
};

// List all supported codecs here. This map defines the command-line parameter
// value (the key string) for selecting each codec, together with information
// whether it is using internal or external DTX/CNG.
const std::map<std::string, CodecTypeAndDtxType> kCodecList = {
    {"opus", {CodecType::kOpus, true}},
    {"pcmu", {CodecType::kPcmU, false}},
    {"pcma", {CodecType::kPcmA, false}},
    {"g722", {CodecType::kG722, false}},
    {"pcm16b_8", {CodecType::kPcm16b8, false}},
    {"pcm16b_16", {CodecType::kPcm16b16, false}},
    {"pcm16b_32", {CodecType::kPcm16b32, false}},
    {"pcm16b_48", {CodecType::kPcm16b48, false}},
    {"ilbc", {CodecType::kIlbc, false}},
    {"isac", {CodecType::kIsac, false}}};

// This class will receive callbacks from ACM when a packet is ready, and write
// it to the output file.
class Packetizer : public AudioPacketizationCallback {
 public:
  Packetizer(FILE* out_file, uint32_t ssrc, int timestamp_rate_hz)
      : out_file_(out_file),
        ssrc_(ssrc),
        timestamp_rate_hz_(timestamp_rate_hz) {}
  ~Packetizer() override = default;

  int32_t SendData(FrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_len_bytes,
                   const RTPFragmentationHeader* fragmentation) override {
    RTC_CHECK(!fragmentation);
    RTC_DCHECK_GT(payload_len_bytes, 0);

    constexpr size_t kRtpHeaderLength = 12;
    constexpr size_t kRtpDumpHeaderLength = 8;
    const uint16_t length = htons(rtc::checked_cast<uint16_t>(
        kRtpHeaderLength + kRtpDumpHeaderLength + payload_len_bytes));
    const uint16_t plen = htons(
        rtc::checked_cast<uint16_t>(kRtpHeaderLength + payload_len_bytes));
    const uint32_t offset = htonl(timestamp / (timestamp_rate_hz_ / 1000));
    RTC_CHECK_EQ(fwrite(&length, sizeof(uint16_t), 1, out_file_), 1);
    RTC_CHECK_EQ(fwrite(&plen, sizeof(uint16_t), 1, out_file_), 1);
    RTC_CHECK_EQ(fwrite(&offset, sizeof(uint32_t), 1, out_file_), 1);

    uint8_t rtp_header[kRtpHeaderLength];
    rtp_header[0] = 0x80;
    rtp_header[1] = payload_type & 0xFF;
    rtp_header[2] = (sequence_number_ >> 8) & 0xFF;
    rtp_header[3] = sequence_number_ & 0xFF;
    rtp_header[4] = timestamp >> 24;
    rtp_header[5] = (timestamp >> 16) & 0xFF;
    rtp_header[6] = (timestamp >> 8) & 0xFF;
    rtp_header[7] = timestamp & 0xFF;
    rtp_header[8] = ssrc_ >> 24;
    rtp_header[9] = (ssrc_ >> 16) & 0xFF;
    rtp_header[10] = (ssrc_ >> 8) & 0xFF;
    rtp_header[11] = ssrc_ & 0xFF;
    RTC_CHECK_EQ(
        fwrite(rtp_header, sizeof(uint8_t), kRtpHeaderLength, out_file_),
        kRtpHeaderLength);
    ++sequence_number_;

    RTC_CHECK_EQ(
        fwrite(payload_data, sizeof(uint8_t), payload_len_bytes, out_file_),
        payload_len_bytes);

    return 0;
  }

 private:
  FILE* const out_file_;
  const uint32_t ssrc_;
  const int timestamp_rate_hz_;
  int16_t sequence_number_ = 0;
};

void SetFrameLenIfPositive(int* config_frame_len) {
  if (FLAG_frame_len > 0) {
    *config_frame_len = FLAG_frame_len;
  }
}

int FlagPayloadTypeOr(int default_pt) {
  return FLAG_payload_type == -1 ? default_pt : FLAG_payload_type;
}

AudioEncoderPcm16B::Config Pcm16bConfig(CodecType codec_type) {
  AudioEncoderPcm16B::Config config;
  SetFrameLenIfPositive(&config.frame_size_ms);
  switch (codec_type) {
    case CodecType::kPcm16b8:
      config.sample_rate_hz = 8000;
      config.payload_type = FlagPayloadTypeOr(93);
      break;
    case CodecType::kPcm16b16:
      config.sample_rate_hz = 16000;
      config.payload_type = FlagPayloadTypeOr(94);
      break;
    case CodecType::kPcm16b32:
      config.sample_rate_hz = 32000;
      config.payload_type = FlagPayloadTypeOr(95);
      break;
    case CodecType::kPcm16b48:
      config.sample_rate_hz = 48000;
      config.payload_type = FlagPayloadTypeOr(96);
      break;
    default:
      RTC_NOTREACHED();
  }
  return config;
}

int RunRtpEncode(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for generating an RTP dump file from audio input.\n"
      "Run " +
      program_name +
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.pcm output.rtp --codec=[codec] " +
      "--frame_len=[frame_len] --bitrate=[bitrate]\n\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) || FLAG_help ||
      (!FLAG_list_codecs && argc != 3)) {
    printf("%s", usage.c_str());
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }

  if (FLAG_list_codecs) {
    printf("The following arguments are valid --codec parameters:\n");
    for (const auto& c : kCodecList) {
      printf("  %s\n", c.first.c_str());
    }
    return 0;
  }

  auto codec_it = kCodecList.find(FLAG_codec);
  if (codec_it == kCodecList.end()) {
    printf("%s is not a valid codec name.\n", FLAG_codec);
    printf("Use argument --list_codecs to see all valid codec names.\n");
    return 1;
  }

  const CodecType codec_type = codec_it->second.type;

  // Create the codec.
  std::unique_ptr<AudioEncoder> codec;
  switch (codec_type) {
    case CodecType::kOpus: {
      AudioEncoderOpusConfig config;
      if (FLAG_bitrate > 0) {
        config.bitrate_bps = rtc::Optional<int>(FLAG_bitrate);
      }
      config.dtx_enabled = FLAG_dtx;

      SetFrameLenIfPositive(&config.frame_size_ms);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderOpusImpl(config, FlagPayloadTypeOr(111)));
      break;
    }

    case CodecType::kPcmU: {
      AudioEncoderPcmU::Config config;
      SetFrameLenIfPositive(&config.frame_size_ms);
      config.payload_type = FlagPayloadTypeOr(config.payload_type);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderPcmU(config));
      break;
    }

    case CodecType::kPcmA: {
      AudioEncoderPcmA::Config config;
      SetFrameLenIfPositive(&config.frame_size_ms);
      config.payload_type = FlagPayloadTypeOr(config.payload_type);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderPcmA(config));
      break;
    }

    case CodecType::kG722: {
      AudioEncoderG722Config config;
      SetFrameLenIfPositive(&config.frame_size_ms);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderG722Impl(config, FlagPayloadTypeOr(9)));
      break;
    }

    case CodecType::kPcm16b8:
    case CodecType::kPcm16b16:
    case CodecType::kPcm16b32:
    case CodecType::kPcm16b48: {
      codec.reset(new AudioEncoderPcm16B(Pcm16bConfig(codec_type)));
      break;
    }

    case CodecType::kIlbc: {
      AudioEncoderIlbcConfig config;
      SetFrameLenIfPositive(&config.frame_size_ms);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderIlbcImpl(config, FlagPayloadTypeOr(102)));
      break;
    }

    case CodecType::kIsac: {
      printf("Codec iSAC\n");
      AudioEncoderIsacFloatImpl::Config config;
      SetFrameLenIfPositive(&config.frame_size_ms);
      config.payload_type = FlagPayloadTypeOr(config.payload_type);
      RTC_CHECK(config.IsOk());
      codec.reset(new AudioEncoderIsacFloatImpl(config));
      break;
    }
  }

  if (FLAG_dtx && codec_it->second.internal_dtx) {
    AudioEncoderCng::Config cng_config;
    switch (codec->SampleRateHz()) {
      case 8000:
        cng_config.payload_type =
            FLAG_cng_payload_type != -1 ? FLAG_cng_payload_type : 13;
        break;
      case 16000:
        cng_config.payload_type =
            FLAG_cng_payload_type != -1 ? FLAG_cng_payload_type : 98;
        break;
      case 32000:
        cng_config.payload_type =
            FLAG_cng_payload_type != -1 ? FLAG_cng_payload_type : 99;
        break;
      case 48000:
        cng_config.payload_type =
            FLAG_cng_payload_type != -1 ? FLAG_cng_payload_type : 100;
        break;
      default:
        RTC_NOTREACHED();
    }
    RTC_DCHECK(codec);
    cng_config.speech_encoder = std::move(codec);
    codec.reset(new AudioEncoderCng(std::move(cng_config)));
  }
  RTC_DCHECK(codec);

  const int timestamp_rate_hz = codec->RtpTimestampRateHz();
  AudioCodingModule::Config config;
  std::unique_ptr<AudioCodingModule> acm(AudioCodingModule::Create(config));
  acm->SetEncoder(std::move(codec));

  printf("Input file: %s\n", argv[1]);
  InputAudioFile input_file(argv[1], false);

  FILE* out_file = fopen(argv[2], "wb");
  RTC_CHECK(out_file) << "Could not open file " << argv[2] << " for writing";
  printf("Output file: %s\n", argv[2]);

  fprintf(out_file, "#!rtpplay1.0 \n");  //,
  uint32_t dummy_variable = 0;  // should be converted to network endian format,
                                // but does not matter when 0
  RTC_CHECK_EQ(fwrite(&dummy_variable, sizeof(uint32_t), 1, out_file), 1);
  RTC_CHECK_EQ(fwrite(&dummy_variable, sizeof(uint32_t), 1, out_file), 1);
  RTC_CHECK_EQ(fwrite(&dummy_variable, sizeof(uint32_t), 1, out_file), 1);
  RTC_CHECK_EQ(fwrite(&dummy_variable, sizeof(uint16_t), 1, out_file), 1);
  RTC_CHECK_EQ(fwrite(&dummy_variable, sizeof(uint16_t), 1, out_file), 1);

  Packetizer packetizer(out_file, FLAG_ssrc, timestamp_rate_hz);
  RTC_DCHECK_EQ(acm->RegisterTransportCallback(&packetizer), 0);

  AudioFrame audio_frame;
  audio_frame.samples_per_channel_ = FLAG_sample_rate / 100;  // 10 ms
  audio_frame.sample_rate_hz_ = FLAG_sample_rate;
  audio_frame.num_channels_ = 1;

  while (input_file.Read(audio_frame.samples_per_channel_,
                         audio_frame.mutable_data())) {
    RTC_CHECK_GE(acm->Add10MsData(audio_frame), 0);
    audio_frame.timestamp_ += audio_frame.samples_per_channel_;
  }

  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::RunRtpEncode(argc, argv);
}
