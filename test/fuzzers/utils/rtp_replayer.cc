/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/utils/rtp_replayer.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {
namespace {

class FileRenderPassthrough : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FileRenderPassthrough(const std::string& basename,
                        rtc::VideoSinkInterface<VideoFrame>* renderer)
      : basename_(basename), renderer_(renderer), file_(nullptr), count_(0) {}

  ~FileRenderPassthrough() override {
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
  ~DecoderBitstreamFileWriter() override { fclose(file_); }

  int32_t Decode(const EncodedImage& encoded_frame,
                 bool /* missing_frames */,
                 const CodecSpecificInfo* /* codec_specific_info */,
                 int64_t /* render_time_ms */) override {
    if (fwrite(encoded_frame.data(), 1, encoded_frame.size(), file_) <
        encoded_frame.size()) {
      RTC_LOG_ERR(LS_ERROR) << "fwrite of encoded frame failed.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  FILE* file_;
};

}  // namespace

void RtpReplayer::Replay(const std::string& replay_config,
                         const uint8_t* rtp_dump_data,
                         size_t rtp_dump_size) {
  // Attempt to create an RtpReader from the input file.
  std::unique_ptr<test::RtpFileReader> rtp_reader =
      CreateRtpReader(rtp_dump_data, rtp_dump_size);
  if (rtp_reader == nullptr) {
    return;
  }

  webrtc::RtcEventLogNullImpl event_log;
  Call::Config call_config(&event_log);
  std::unique_ptr<Call> call(Call::Create(call_config));
  std::unique_ptr<StreamState> stream_state;
  stream_state = ConfigureForFuzzer(replay_config, call.get());
  if (stream_state == nullptr) {
    return;
  }
  // Start replaying the provided stream now that it has been configured.
  for (const auto& receive_stream : stream_state->receive_streams) {
    receive_stream->Start();
  }
  ReplayPackets(call.get(), rtp_reader.get());
  for (const auto& receive_stream : stream_state->receive_streams) {
    call->DestroyVideoReceiveStream(receive_stream);
  }
}

std::unique_ptr<RtpReplayer::StreamState> RtpReplayer::ConfigureForFuzzer(
    const std::string& replay_config,
    Call* call) {
  auto stream_state = absl::make_unique<StreamState>();
  // Parse the configuration file.
  Json::Reader json_reader;
  Json::Value json_configs;
  if (!json_reader.parse(replay_config, json_configs)) {
    fprintf(stderr, "Error parsing JSON config\n");
    fprintf(stderr, "%s\n", json_reader.getFormatedErrorMessages().c_str());
    return nullptr;
  }

  stream_state->decoder_factory = absl::make_unique<InternalDecoderFactory>();
  size_t config_count = 0;
  for (const auto& json : json_configs) {
    // Create the configuration and parse the JSON into the config.
    auto receive_config =
        ParseVideoReceiveStreamJsonConfig(&(stream_state->transport), json);
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
        call->CreateVideoReceiveStream(std::move(receive_config)));
  }
  return stream_state;
}

std::unique_ptr<test::RtpFileReader> RtpReplayer::CreateRtpReader(
    const uint8_t* rtp_dump_data,
    size_t rtp_dump_size) {
  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, rtp_dump_data, rtp_dump_size, {}));
  if (!rtp_reader) {
    fprintf(stderr, "Unable to open input file with any supported format\n");
    return nullptr;
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
