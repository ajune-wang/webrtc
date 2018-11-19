/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_dump_replayer.h"

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/json.h"
#include "system_wrappers/include/sleep.h"
#include "test/encoder_settings.h"
#include "test/null_transport.h"
#include "test/rtp_dump_parser.h"
#include "test/video_json_config.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace test {

RtpDumpReplayer::StreamState::StreamState() {}

RtpDumpReplayer::StreamState::~StreamState() {}

std::unique_ptr<RtpDumpReplayer::StreamState>
RtpDumpReplayer::StreamState::Load(const std::string& config_path) {
  // Parse the configuration file.
  std::ifstream config_file(config_path);
  std::stringstream raw_json_buffer;
  raw_json_buffer << config_file.rdbuf();
  return FromString(raw_json_buffer.str());
}

std::unique_ptr<RtpDumpReplayer::StreamState>
RtpDumpReplayer::StreamState::FromString(const std::string& config_string) {
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
    RTC_LOG(LS_ERROR) << "Error parsing JSON config: "
                      << json_reader.getFormatedErrorMessages();
    return nullptr;
  }

  stream_state->decoder_factory = absl::make_unique<InternalDecoderFactory>();
  size_t config_count = 0;
  for (const auto& json : json_configs) {
    // Create the configuration and parse the JSON into the config.
    auto receive_config =
        JsonToVideoReceiveStreamConfig(&(stream_state->transport), json);
    // Instantiate the underlying decoder.
    for (auto& decoder : receive_config.decoders) {
      decoder = test::CreateMatchingDecoder(decoder.payload_type,
                                            decoder.video_format.name);
      decoder.decoder_factory = stream_state->decoder_factory.get();
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

void RtpDumpReplayer::Replay(std::unique_ptr<StreamState> stream_state,
                             rtc::ArrayView<const uint8_t> rtp_dump_buffer) {
  if (stream_state == nullptr) {
    RTC_LOG(LS_ERROR) << "Provided stream state is null";
    return;
  }
  // Start replaying the provided stream now that it has been configured.
  for (VideoReceiveStream* receive_stream : stream_state->receive_streams) {
    receive_stream->Start();
  }
  ReplayPackets(stream_state->call.get(), rtp_dump_buffer);
  for (VideoReceiveStream* receive_stream : stream_state->receive_streams) {
    stream_state->call->DestroyVideoReceiveStream(receive_stream);
  }
}

void RtpDumpReplayer::ReplayPackets(
    Call* call,
    rtc::ArrayView<const uint8_t> rtp_dump_buffer) {
  int64_t replay_start_ms = -1;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;

  auto rtp_dump_parser = RtpDumpParser::Create(rtp_dump_buffer);
  if (rtp_dump_parser == nullptr) {
    RTC_LOG(LS_ERROR) << "Unable to create RtpDumpParser";
    return;
  }

  while (true) {
    int64_t now_ms = rtc::TimeMillis();
    if (replay_start_ms == -1) {
      replay_start_ms = now_ms;
    }

    test::RtpPacket packet;
    if (!rtp_dump_parser->NextPacket(&packet)) {
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
        if (unknown_packets[header.ssrc] == 0) {
          RTC_LOG(LS_ERROR) << "Unknown SSRC: " << header.ssrc;
        }
        ++unknown_packets[header.ssrc];
        break;
      }
      case PacketReceiver::DELIVERY_PACKET_ERROR: {
        RTC_LOG(LS_ERROR)
            << "Packet error, corrupt packets or incorrect setup?";
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        RTC_LOG(LS_ERROR) << "Packet len=" << packet.length
                          << "Payload type=" << header.payloadType
                          << "Sequence Number=" << header.sequenceNumber
                          << "Timestamp=" << header.timestamp
                          << "SSRC=" << header.ssrc;
        break;
      }
    }
  }
  RTC_LOG(LS_ERROR) << "num_packets: " << num_packets;

  for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
       it != unknown_packets.end(); ++it) {
    RTC_LOG(LS_ERROR) << "Packets for unknown ssrc " << it->first << it->second;
  }
}

}  // namespace test
}  // namespace webrtc
