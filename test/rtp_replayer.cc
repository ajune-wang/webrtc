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
#include <string>
#include <utility>
#include <vector>

#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/json.h"
#include "system_wrappers/include/sleep.h"
#include "test/encoder_settings.h"
#include "test/null_transport.h"
#include "test/rtp_replayer.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace test {
namespace {

// Deserializes a JSON representation of the VideoReceiveStream::Config back
// into a valid object. This will not initialize the decoders or the renderer.
class VideoReceiveStreamConfigDeserializer final {
 public:
  static VideoReceiveStream::Config Deserialize(webrtc::Transport* transport,
                                                const Json::Value& json) {
    auto receive_config = VideoReceiveStream::Config(transport);
    for (const auto decoder_json : json["decoders"]) {
      VideoReceiveStream::Decoder decoder;
      decoder.payload_name = decoder_json["payload_name"].asString();
      decoder.payload_type = decoder_json["payload_type"].asInt64();
      for (const auto& params_json : decoder_json["codec_params"]) {
        std::vector<std::string> members = params_json.getMemberNames();
        RTC_CHECK_EQ(members.size(), 1);
        decoder.codec_params[members[0]] = params_json[members[0]].asString();
      }
      receive_config.decoders.push_back(decoder);
    }
    receive_config.render_delay_ms = json["render_delay_ms"].asInt64();
    receive_config.target_delay_ms = json["target_delay_ms"].asInt64();
    receive_config.rtp.remote_ssrc = json["remote_ssrc"].asInt64();
    receive_config.rtp.local_ssrc = json["local_ssrc"].asInt64();
    receive_config.rtp.rtcp_mode =
        json["rtcp_mode"].asString() == "RtcpMode::kCompound"
            ? RtcpMode::kCompound
            : RtcpMode::kReducedSize;
    receive_config.rtp.remb = json["remb"].asBool();
    receive_config.rtp.transport_cc = json["transport_cc"].asBool();
    receive_config.rtp.nack.rtp_history_ms =
        json["nack"]["rtp_history_ms"].asInt64();
    receive_config.rtp.ulpfec_payload_type =
        json["ulpfec_payload_type"].asInt64();
    receive_config.rtp.red_payload_type = json["red_payload_type"].asInt64();
    receive_config.rtp.rtx_ssrc = json["rtx_ssrc"].asInt64();

    for (const auto& pl_json : json["rtx_payload_types"]) {
      std::vector<std::string> members = pl_json.getMemberNames();
      RTC_CHECK_EQ(members.size(), 1);
      Json::Value rtx_payload_type = pl_json[members[0]];
      receive_config.rtp.rtx_associated_payload_types[std::stoi(members[0])] =
          rtx_payload_type.asInt64();
    }
    for (const auto& ext_json : json["extensions"]) {
      receive_config.rtp.extensions.emplace_back(ext_json["uri"].asString(),
                                                 ext_json["id"].asInt64(),
                                                 ext_json["encrypt"].asBool());
    }
    return receive_config;
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RtpReplayer Implementation
////////////////////////////////////////////////////////////////////////////////

RtpReplayer::StreamState::StreamState() {}

RtpReplayer::StreamState::~StreamState() {}

std::unique_ptr<RtpReplayer::StreamState> RtpReplayer::StreamState::Load(
    const std::string& config_path) {
  // Parse the configuration file.
  std::ifstream config_file(config_path);
  std::stringstream raw_json_buffer;
  raw_json_buffer << config_file.rdbuf();
  return FromString(raw_json_buffer.str());
}

std::unique_ptr<RtpReplayer::StreamState> RtpReplayer::StreamState::FromString(
    const std::string& config_string) {
  auto stream_state = absl::make_unique<StreamState>();
  // Initalize the call.
  webrtc::RtcEventLogNullImpl event_log;
  Call::Config call_config(&event_log);
  std::unique_ptr<Call> call(Call::Create(std::move(call_config)));
  stream_state->call = std::move(call);

  std::string raw_json = config_string;
  Json::Reader json_reader;
  Json::Value json_configs;
  if (!json_reader.parse(raw_json, json_configs)) {
    fprintf(stderr, "Error parsing JSON config\n");
    fprintf(stderr, "%s\n", json_reader.getFormatedErrorMessages().c_str());
    return nullptr;
  }

  size_t config_count = 0;
  for (const auto& json : json_configs) {
    // Create the configuration and parse the JSON into the config.
    auto receive_config = VideoReceiveStreamConfigDeserializer::Deserialize(
        &(stream_state->transport), json);
    // Instantiate the underlying decoder.
    for (auto& decoder : receive_config.decoders) {
      decoder.decoder = test::CreateMatchingDecoder(decoder.payload_type,
                                                    decoder.payload_name)
                            .decoder;
    }
    // Create a window for this config.
    std::stringstream window_title;
    window_title << "Playback Video (" << config_count++ << ")";
    stream_state->sinks.emplace_back(
        test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
    // Create a receive stream for this config.
    receive_config.renderer = stream_state->sinks.back().get();
    stream_state->receive_streams.emplace_back(
        stream_state->call->CreateVideoReceiveStream(
            std::move(receive_config)));
  }
  return stream_state;
}

void RtpReplayer::Replay(std::unique_ptr<StreamState> stream_state,
                         const std::string& rtp_dump_path) {
  if (stream_state == nullptr) {
    fprintf(stderr, "Provided stream state is null\n");
    return;
  }
  // Attempt to create an RtpReader from the input file.
  std::unique_ptr<test::RtpFileReader> rtp_reader =
      CreateRtpReader(rtp_dump_path);
  if (rtp_reader == nullptr) {
    fprintf(stderr, "Could not create RtpReader\n");
    return;
  }
  // Start replaying the provided stream now that it has been configured.
  for (VideoReceiveStream* receive_stream : stream_state->receive_streams) {
    receive_stream->Start();
  }
  ReplayPackets(stream_state->call.get(), rtp_reader.get());
  for (VideoReceiveStream* receive_stream : stream_state->receive_streams) {
    stream_state->call->DestroyVideoReceiveStream(receive_stream);
  }
}

std::unique_ptr<test::RtpFileReader> RtpReplayer::CreateRtpReader(
    const std::string& rtp_dump_path) {
  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, rtp_dump_path));
  if (!rtp_reader) {
    rtp_reader.reset(
        test::RtpFileReader::Create(test::RtpFileReader::kPcap, rtp_dump_path));
    if (!rtp_reader) {
      fprintf(stderr,
              "Couldn't open input file as either a rtpdump or .pcap. Note "
              "that .pcapng is not supported.\nTrying to interpret the file as "
              "length/packet interleaved.\n");
      rtp_reader.reset(test::RtpFileReader::Create(
          test::RtpFileReader::kLengthPacketInterleaved, rtp_dump_path));
      if (!rtp_reader) {
        fprintf(stderr,
                "Unable to open input file with any supported format\n");
        return nullptr;
      }
    }
  }
  return rtp_reader;
}

void RtpReplayer::ReplayPackets(Call* call, test::RtpFileReader* rtp_reader) {
  int64_t replay_start_ms = -1;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;

  while (true) {
    int64_t now_ms = rtc::TimeMillis();
    if (replay_start_ms == -1) {
      replay_start_ms = now_ms;
    }

    test::RtpPacket packet;
    if (!rtp_reader->NextPacket(&packet)) {
      break;
    }

    int64_t deliver_in_ms = replay_start_ms + packet.time_ms - now_ms;
    if (deliver_in_ms > 0) {
      SleepMs(deliver_in_ms);
    }

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
}

}  // namespace test
}  // namespace webrtc
