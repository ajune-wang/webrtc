/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <fstream>
#include <map>
#include <memory>
#include <sstream>  // no-presubmit-check TODO(webrtc:8982)

#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/json.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtp_file_reader.h"
#include "test/run_loop.h"
#include "test/run_test.h"
#include "test/testsupport/frame_writer.h"
#include "test/video_capturer.h"
#include "test/video_renderer.h"

namespace {

static bool ValidatePayloadType(int32_t payload_type) {
  return payload_type > 0 && payload_type <= 127;
}

static bool ValidateSsrc(const char* ssrc_string) {
  return rtc::StringToNumber<uint32_t>(ssrc_string).has_value();
}

static bool ValidateOptionalPayloadType(int32_t payload_type) {
  return payload_type == -1 || ValidatePayloadType(payload_type);
}

static bool ValidateRtpHeaderExtensionId(int32_t extension_id) {
  return extension_id >= -1 && extension_id < 15;
}

bool ValidateInputFilenameNotEmpty(const std::string& string) {
  return !string.empty();
}

}  // namespace

namespace webrtc {
namespace flags {

// TODO(pbos): Multiple receivers.

// Flag for payload type.
DEFINE_int(media_payload_type,
           test::CallTest::kPayloadTypeVP8,
           "Media payload type");
static int MediaPayloadType() {
  return static_cast<int>(FLAG_media_payload_type);
}

// Flag for RED payload type.
DEFINE_int(red_payload_type,
           test::CallTest::kRedPayloadType,
           "RED payload type");
static int RedPayloadType() {
  return static_cast<int>(FLAG_red_payload_type);
}

// Flag for ULPFEC payload type.
DEFINE_int(ulpfec_payload_type,
           test::CallTest::kUlpfecPayloadType,
           "ULPFEC payload type");
static int UlpfecPayloadType() {
  return static_cast<int>(FLAG_ulpfec_payload_type);
}

DEFINE_int(media_payload_type_rtx,
           test::CallTest::kSendRtxPayloadType,
           "Media over RTX payload type");
static int MediaPayloadTypeRtx() {
  return static_cast<int>(FLAG_media_payload_type_rtx);
}

DEFINE_int(red_payload_type_rtx,
           test::CallTest::kRtxRedPayloadType,
           "RED over RTX payload type");
static int RedPayloadTypeRtx() {
  return static_cast<int>(FLAG_red_payload_type_rtx);
}

// Flag for SSRC.
const std::string& DefaultSsrc() {
  static const std::string ssrc =
      std::to_string(test::CallTest::kVideoSendSsrcs[0]);
  return ssrc;
}
DEFINE_string(ssrc, DefaultSsrc().c_str(), "Incoming SSRC");
static uint32_t Ssrc() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc).value();
}

const std::string& DefaultSsrcRtx() {
  static const std::string ssrc_rtx =
      std::to_string(test::CallTest::kSendRtxSsrcs[0]);
  return ssrc_rtx;
}
DEFINE_string(ssrc_rtx, DefaultSsrcRtx().c_str(), "Incoming RTX SSRC");
static uint32_t SsrcRtx() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc_rtx).value();
}

// Flag for abs-send-time id.
DEFINE_int(abs_send_time_id, -1, "RTP extension ID for abs-send-time");
static int AbsSendTimeId() {
  return static_cast<int>(FLAG_abs_send_time_id);
}

// Flag for transmission-offset id.
DEFINE_int(transmission_offset_id,
           -1,
           "RTP extension ID for transmission-offset");
static int TransmissionOffsetId() {
  return static_cast<int>(FLAG_transmission_offset_id);
}

// Flag for rtpdump input file.
DEFINE_string(input_file, "", "input file");
static std::string InputFile() {
  return static_cast<std::string>(FLAG_input_file);
}

// Flag for config input file.
DEFINE_string(config_file, "", "input file");
static std::string ConfigFile() {
  return static_cast<std::string>(FLAG_config_file);
}

// Flag for raw output files.
DEFINE_string(out_base, "", "Basename (excluding .jpg) for raw output");
static std::string OutBase() {
  return static_cast<std::string>(FLAG_out_base);
}

DEFINE_string(decoder_bitstream_filename, "", "Decoder bitstream output file");
static std::string DecoderBitstreamFilename() {
  return static_cast<std::string>(FLAG_decoder_bitstream_filename);
}

// Flag for video codec.
DEFINE_string(codec, "VP8", "Video codec");
static std::string Codec() {
  return static_cast<std::string>(FLAG_codec);
}

DEFINE_bool(help, false, "Print this message.");
}  // namespace flags

static const uint32_t kReceiverLocalSsrc = 0x123456;

class FileRenderPassthrough : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FileRenderPassthrough(const std::string& basename,
                        rtc::VideoSinkInterface<VideoFrame>* renderer)
      : basename_(basename), renderer_(renderer), file_(nullptr), count_(0) {}

  ~FileRenderPassthrough() {
    if (file_)
      fclose(file_);
  }

 private:
  void OnFrame(const VideoFrame& video_frame) override {
    if (renderer_)
      renderer_->OnFrame(video_frame);

    if (basename_.empty())
      return;

    std::stringstream filename;
    filename << basename_ << count_++ << "_" << video_frame.timestamp()
             << ".jpg";

    test::JpegFrameWriter frame_writer(filename.str());
    RTC_CHECK(frame_writer.WriteFrame(video_frame, 100));
  }

  const std::string basename_;
  rtc::VideoSinkInterface<VideoFrame>* const renderer_;
  FILE* file_;
  size_t count_;
};

class DecoderBitstreamFileWriter : public test::FakeDecoder {
 public:
  explicit DecoderBitstreamFileWriter(const char* filename)
      : file_(fopen(filename, "wb")) {
    RTC_DCHECK(file_);
  }
  ~DecoderBitstreamFileWriter() { fclose(file_); }

  int32_t Decode(const EncodedImage& encoded_frame,
                      bool /* missing_frames */,
                      const CodecSpecificInfo* /* codec_specific_info */,
                      int64_t /* render_time_ms */) override {
    if (fwrite(encoded_frame._buffer, 1, encoded_frame._length, file_)
        < encoded_frame._length) {
      RTC_LOG_ERR(LS_ERROR) << "fwrite of encoded frame failed.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  FILE* file_;
};

class RtpReplayConfig final {
 public:
  static bool Read(VideoReceiveStream::Config* config, std::string config_str) {
    Json::Reader reader;
    Json::Value jmessage;
    config_str = AddQuotes(config_str);
    fprintf(stderr, "%s", config_str.c_str());
    if (!reader.parse(config_str, jmessage)) {
      fprintf(stderr, "parsing json failed");
      return false;
    }
    if (!rtc::GetIntFromJsonObject(jmessage, "render_delay_ms",
                                   &config->render_delay_ms)) {
      RTC_LOG_ERR(LS_WARNING) << "No render delay.";
    }

    if (!rtc::GetIntFromJsonObject(jmessage, "target_delay_ms",
                                   &config->target_delay_ms)) {
      RTC_LOG_ERR(LS_WARNING) << "No target delay.";
    }

    if (!rtc::GetStringFromJsonObject(jmessage, "sync_group",
                                      &config->sync_group)) {
      RTC_LOG_ERR(LS_WARNING) << "No sync_group.";
    }

    Json::Value decoders;

    if (!rtc::GetValueFromJsonObject(jmessage, "decoders", &decoders)) {
      RTC_LOG_ERR(LS_WARNING) << "No decoders.";
    } else {
      ReadDecoder(config, decoders);
    }

    Json::Value rtp;
    if (!rtc::GetValueFromJsonObject(jmessage, "rtp", &rtp)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtp.";
    } else {
      ReadRtp(config, rtp);
    }
    return true;
  }

 private:
  static std::string AddQuotes(std::string in_str) {
    int mode = 0;
    std::string out_str;
    std::size_t found = in_str.find("rtx_payload_types");
    in_str.insert(found - 1 + sizeof("rtx_payload_types: "), "\"");
    found = in_str.find(", }");
    in_str.insert(found - 1 + sizeof(", }"), "\"");
    size_t t = 0;
    size_t found2 = 0;
    while (in_str.find(":", t) != std::string::npos) {
      found = in_str.find(":", t);
      fprintf(stderr, "found %zu\r\n", found);
      for (int j = found; j >= static_cast<int>(found2); j--) {
        if (in_str[j] == '{' || (in_str[j] == ' ' && in_str[j - 1] == ',') ||
            in_str[j] == '[') {
          in_str.insert(found, "\"");
          in_str.insert(j + 1, "\"");
          break;
        }
      }
      found2 = found;
      t = found + 3;
    }
    for (size_t i = 0; i < in_str.length(); i++) {
      if (mode == 0 && in_str[i] != ':') {
        out_str = out_str + in_str[i];
      } else {
        std::string temp;
        temp += in_str[i++];
        temp += in_str[i++];
        if (in_str[i] == '{' || in_str[i] == '[' ||
            (in_str[i] >= '0' && in_str[i] <= '9') || in_str[i] == '"') {
          out_str += temp + in_str[i];
        } else {
          out_str += temp + '"' + in_str[i++];
          while (in_str[i] != ',' && in_str[i] != '}' && in_str[i] != ']') {
            out_str += in_str[i++];
          }
          if (in_str[i] == '}' || in_str[i] == ']') {
            out_str = out_str + '"' + in_str[i];
          } else {
            if (in_str[i] == ',' && in_str[i + 1] == ' ' &&
                in_str[i + 2] == '}') {
              i += 3;
              out_str = out_str + ", }" + '"';
            } else {
              out_str = out_str + "\",";
            }
          }
        }
      }
    }
    return out_str;
  }

  static bool ReadRtp(VideoReceiveStream::Config* config, Json::Value rtp) {
    if (!rtc::GetUIntFromJsonObject(rtp, "remote_ssrc",
                                    &config->rtp.remote_ssrc)) {
      RTC_LOG_ERR(LS_WARNING) << "No remote ssrc.";
    }
    if (!rtc::GetUIntFromJsonObject(rtp, "local_ssrc",
                                    &config->rtp.local_ssrc)) {
      RTC_LOG_ERR(LS_WARNING) << "No localssrc.";
    }

    std::string mode;
    if (!rtc::GetStringFromJsonObject(rtp, "rtcp_mode", &mode)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtcp_mode.";
    }
    if (mode == "RtcpMode::kCompound") {
      config->rtp.rtcp_mode = webrtc::RtcpMode::kCompound;
    } else {
      config->rtp.rtcp_mode = webrtc::RtcpMode::kReducedSize;
    }

    Json::Value rtcp_xr;
    if (!rtc::GetValueFromJsonObject(rtp, "rtcp_xr", &rtcp_xr)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtcp_xr.";
    }

    std::string report;
    if (!rtc::GetStringFromJsonObject(rtcp_xr, "receiver_reference_time_report",
                                      &report)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtcp_mode.";
    }
    config->rtp.rtcp_xr.receiver_reference_time_report = (report == "on");

    std::string remb;
    if (!rtc::GetStringFromJsonObject(rtp, "remb", &remb)) {
      RTC_LOG_ERR(LS_WARNING) << "No remb.";
    }
    config->rtp.remb = (remb == "on");

    std::string transport_cc;
    if (!rtc::GetStringFromJsonObject(rtp, "transport_cc", &transport_cc)) {
      RTC_LOG_ERR(LS_WARNING) << "No transport_cc.";
    }
    config->rtp.transport_cc = (transport_cc == "on");

    Json::Value nack;
    if (!rtc::GetValueFromJsonObject(rtp, "nack", &nack)) {
      RTC_LOG_ERR(LS_WARNING) << "No nack.";
    }
    if (!rtc::GetIntFromJsonObject(nack, "rtp_history_ms",
                                   &config->rtp.nack.rtp_history_ms)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtp_history_ms.";
    }
    if (!rtc::GetIntFromJsonObject(rtp, "ulpfec_payload_type",
                                   &config->rtp.ulpfec_payload_type)) {
      RTC_LOG_ERR(LS_WARNING) << "No ulpfec_payload_type.";
    }
    if (!rtc::GetIntFromJsonObject(rtp, "red_type",
                                   &config->rtp.red_payload_type)) {
      RTC_LOG_ERR(LS_WARNING) << "No red_payload_type";
    }
    if (!rtc::GetUIntFromJsonObject(rtp, "rtx_ssrc", &config->rtp.rtx_ssrc)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtx_ssrc";
    }

    std::string rtx_payload_types;
    if (!rtc::GetStringFromJsonObject(rtp, "rtx_payload_types",
                                      &rtx_payload_types)) {
      RTC_LOG_ERR(LS_WARNING) << "No rtx_payload_types";
    }
    rtx_payload_types.erase(0, 1);
    rtx_payload_types.erase(rtx_payload_types.length() - 3, 3);

    while (rtx_payload_types.length() > 0) {
      int first = stoi(rtx_payload_types);
      if (rtx_payload_types.find("->") != std::string::npos) {
        rtx_payload_types.erase(0, rtx_payload_types.find("->") + 3);
      }
      int second = stoi(rtx_payload_types);
      if (rtx_payload_types.find(")") != std::string::npos) {
        rtx_payload_types.erase(0, rtx_payload_types.find(")") + 3);
      }
      config->rtp.rtx_associated_payload_types.insert(
          std::pair<int, int>(first, second));
    }

    Json::Value extensions;
    if (!rtc::GetValueFromJsonObject(rtp, "extensions", &extensions)) {
      RTC_LOG_ERR(LS_WARNING) << "No extensions";
    }

    std::vector<Json::Value> e;
    if (!rtc::JsonArrayToValueVector(extensions, &e)) {
      RTC_LOG_ERR(LS_WARNING) << "Failed to parse extensions";
    }

    for (size_t i = 0; i < e.size(); i++) {
      int id = 0;
      std::string uri;

      if (!rtc::GetIntFromJsonObject(e[i], "id", &id)) {
        RTC_LOG_ERR(LS_WARNING) << "No id.";
      }
      if (!rtc::GetStringFromJsonObject(e[i], "uri", &uri)) {
        RTC_LOG_ERR(LS_WARNING) << "No uri.";
      }
      // if encrypt is ever true, parsing will
      webrtc::RtpExtension r = webrtc::RtpExtension(uri, id, false);
      config->rtp.extensions.push_back(r);
    }
    return true;
  }

  static bool ReadDecoder(VideoReceiveStream::Config* config,
                          Json::Value decoders) {
    std::vector<Json::Value> v;
    if (!rtc::JsonArrayToValueVector(decoders, &v)) {
      RTC_LOG_ERR(LS_WARNING) << "Failed to parse decoders";
    }
    for (size_t i = 0; i < v.size(); i++) {
      Json::Value codec_params;
      Json::Value decoder = v[i];
      std::string name;
      int type = 0;

      if (!rtc::GetIntFromJsonObject(decoder, "payload_type", &type)) {
        RTC_LOG_ERR(LS_WARNING) << "No type";
      }
      if (!rtc::GetStringFromJsonObject(decoder, "payload_name", &name)) {
        RTC_LOG_ERR(LS_WARNING) << "No name";
      }
      if (!rtc::GetValueFromJsonObject(decoder, "codec_params",
                                       &codec_params)) {
        RTC_LOG_ERR(LS_WARNING) << "No params";
      }

      VideoReceiveStream::Decoder cdecoder;
      cdecoder = test::CreateMatchingDecoder(type, name);
      std::vector<std::string> names;
      names = codec_params.getMemberNames();

      for (size_t j = 0; j < names.size(); j++) {
        std::string prop = names[j];
        std::string value;
        if (!rtc::GetStringFromJsonObject(codec_params, prop, &value)) {
          int num;
          if (!rtc::GetIntFromJsonObject(codec_params, prop, &num)) {
            RTC_LOG_ERR(LS_WARNING) << "Failed to get prop: " << prop;
          } else {
            value = std::to_string(num);
          }
        }
        cdecoder.codec_params.insert(
            std::pair<std::string, std::string>(prop, value));
      }

      config->decoders.push_back(cdecoder);
    }
    return true;
  }
};  // RtpReplayConfig

void RtpReplay() {
  std::stringstream window_title;
  window_title << "Playback Video (" << flags::InputFile() << ")";
  std::unique_ptr<test::VideoRenderer> playback_video(
      test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
  FileRenderPassthrough file_passthrough(flags::OutBase(),
                                         playback_video.get());

  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<Call> call(Call::Create(Call::Config(&event_log)));
  test::NullTransport transport;
  VideoReceiveStream::Config receive_config(&transport);

  if (flags::ConfigFile() != "") {
    std::ifstream config_file(flags::ConfigFile().c_str());
    std::stringstream config_buffer;
    config_buffer << config_file.rdbuf();
    std::string config_string = config_buffer.str();
    if (!RtpReplayConfig::Read(&receive_config, config_string)) {
      return;
    }
    receive_config.renderer = &file_passthrough;
  } else {
    receive_config.rtp.remote_ssrc = flags::Ssrc();
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_config.rtp.rtx_ssrc = flags::SsrcRtx();
    receive_config.rtp
        .rtx_associated_payload_types[flags::MediaPayloadTypeRtx()] =
        flags::MediaPayloadType();
    receive_config.rtp
        .rtx_associated_payload_types[flags::RedPayloadTypeRtx()] =
        flags::RedPayloadType();
    receive_config.rtp.ulpfec_payload_type = flags::UlpfecPayloadType();
    receive_config.rtp.red_payload_type = flags::RedPayloadType();
    receive_config.rtp.nack.rtp_history_ms = 1000;
    if (flags::TransmissionOffsetId() != -1) {
      receive_config.rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTimestampOffsetUri, flags::TransmissionOffsetId()));
    }
    if (flags::AbsSendTimeId() != -1) {
      receive_config.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTimeUri, flags::AbsSendTimeId()));
    }
    receive_config.renderer = &file_passthrough;
  }
  VideoReceiveStream::Decoder decoder;
  decoder =
      test::CreateMatchingDecoder(flags::MediaPayloadType(), flags::Codec());
  if (!flags::DecoderBitstreamFilename().empty()) {
    // Replace decoder with file writer if we're writing the bitstream to a file
    // instead.
    delete decoder.decoder;
    decoder.decoder = new DecoderBitstreamFileWriter(
        flags::DecoderBitstreamFilename().c_str());
  }
  receive_config.decoders.push_back(decoder);

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(std::move(receive_config));

  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, flags::InputFile()));
  if (!rtp_reader) {
    rtp_reader.reset(test::RtpFileReader::Create(test::RtpFileReader::kPcap,
                                                 flags::InputFile()));
    if (!rtp_reader) {
      fprintf(stderr,
              "Couldn't open input file as either a rtpdump or .pcap. Note "
              "that .pcapng is not supported.\nTrying to interpret the file as "
              "length/packet interleaved.\n");
      rtp_reader.reset(test::RtpFileReader::Create(
          test::RtpFileReader::kLengthPacketInterleaved, flags::InputFile()));
      if (!rtp_reader) {
        fprintf(stderr,
                "Unable to open input file with any supported format\n");
        return;
      }
    }
  }
  receive_stream->Start();

  int64_t replay_start_ms = -1;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;
  while (true) {
    int64_t now_ms = rtc::TimeMillis();
    if (replay_start_ms == -1)
      replay_start_ms = now_ms;

    test::RtpPacket packet;
    if (!rtp_reader->NextPacket(&packet))
      break;

    int64_t deliver_in_ms = replay_start_ms + packet.time_ms - now_ms;
    if (deliver_in_ms > 0)
      SleepMs(deliver_in_ms);

    ++num_packets;
    switch (call->Receiver()->DeliverPacket(
        webrtc::MediaType::VIDEO,
        rtc::CopyOnWriteBuffer(packet.data, packet.length),
        /* packet_time_us */ -1)) {
      case PacketReceiver::DELIVERY_OK:
        break;
      case PacketReceiver::DELIVERY_UNKNOWN_SSRC: {
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        if (unknown_packets[header.ssrc] == 0)
          fprintf(stderr, "Unknown SSRC: %u!\n", header.ssrc);
        ++unknown_packets[header.ssrc];
        break;
      }
      case PacketReceiver::DELIVERY_PACKET_ERROR: {
        fprintf(stderr, "Packet error, corrupt packets or incorrect setup?\n");
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        fprintf(stderr, "Packet len=%zu pt=%u seq=%u ts=%u ssrc=0x%8x\n",
                packet.length, header.payloadType, header.sequenceNumber,
                header.timestamp, header.ssrc);
        break;
      }
    }
  }
  fprintf(stderr, "num_packets: %d\n", num_packets);

  for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
       it != unknown_packets.end(); ++it) {
    fprintf(stderr, "Packets for unknown ssrc '%u': %d\n", it->first,
            it->second);
  }

  call->DestroyVideoReceiveStream(receive_stream);

  delete decoder.decoder;
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    return 1;
  }
  if (webrtc::flags::FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_media_payload_type));
  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_media_payload_type_rtx));
  RTC_CHECK(ValidateOptionalPayloadType(webrtc::flags::FLAG_red_payload_type));
  RTC_CHECK(
      ValidateOptionalPayloadType(webrtc::flags::FLAG_red_payload_type_rtx));
  RTC_CHECK(
      ValidateOptionalPayloadType(webrtc::flags::FLAG_ulpfec_payload_type));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc_rtx));
  RTC_CHECK(ValidateRtpHeaderExtensionId(webrtc::flags::FLAG_abs_send_time_id));
  RTC_CHECK(
      ValidateRtpHeaderExtensionId(webrtc::flags::FLAG_transmission_offset_id));
  RTC_CHECK(ValidateInputFilenameNotEmpty(webrtc::flags::FLAG_input_file));

  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
