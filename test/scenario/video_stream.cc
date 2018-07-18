/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/video_stream.h"

#include <utility>

#include "test/call_test.h"
#include "test/fake_encoder.h"
#include "test/function_video_encoder_factory.h"

namespace webrtc {
namespace test {
namespace {
const int kVideoRotationRtpExtensionId = 4;
struct CodecInfo {
  VideoCodecType codec_type;
  std::string payload_name;
  int payload_type;
};
CodecInfo GetCodecInfo(VideoStreamConfig config) {
  if (config.encoder.codec == VideoStreamConfig::Encoder::Codec::kFake) {
    return {VideoCodecType::kVideoCodecGeneric, "FAKE",
            CallTest::kFakeVideoSendPayloadType};
  }
  RTC_NOTREACHED();
  return {};
}
std::vector<RtpExtension> GetVideoRtpExtensions(
    const VideoStreamConfig config) {
  return {RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                       kTransportSequenceNumberExtensionId),
          RtpExtension(RtpExtension::kVideoContentTypeUri,
                       kVideoContentTypeExtensionId),
          RtpExtension(RtpExtension::kVideoRotationUri,
                       kVideoRotationRtpExtensionId)};
}
}  // namespace

SendVideoStream::SendVideoStream(CallClient* sender,
                                 VideoStreamConfig config,
                                 Transport* send_transport)
    : sender_(sender) {
  VideoSendStream::Config send_config(send_transport);
  VideoEncoderConfig encoder_config;
  if (config.encoder.codec == VideoStreamConfig::Encoder::Codec::kFake) {
    auto codec_info = GetCodecInfo(config);
    encoder_config.codec_type = codec_info.codec_type;
    encoder_factory_ =
        absl::make_unique<FunctionVideoEncoderFactory>([this, config]() {
          auto encoder = absl::make_unique<test::FakeEncoder>(sender_->clock_);
          if (config.encoder.fake.max_rate.IsFinite())
            encoder->SetMaxBitrate(config.encoder.fake.max_rate.kbps());
          return encoder;
        });
    send_config.rtp.payload_name = codec_info.payload_name;
    send_config.rtp.payload_type = codec_info.payload_type;
  }
  RTC_CHECK(encoder_factory_);
  send_config.encoder_settings.encoder_factory = encoder_factory_.get();

  send_config.rtp.ssrcs = config.stream.ssrcs;
  send_config.rtp.extensions = GetVideoRtpExtensions(config);

  if (config.stream.use_flexfec) {
    send_config.rtp.flexfec.payload_type = CallTest::kFlexfecPayloadType;
    send_config.rtp.flexfec.ssrc = CallTest::kFlexfecSendSsrc;
    send_config.rtp.flexfec.protected_media_ssrcs = config.stream.ssrcs;
  }
  if (config.stream.use_ulpfec) {
    send_config.rtp.ulpfec.red_payload_type = CallTest::kRedPayloadType;
    send_config.rtp.ulpfec.ulpfec_payload_type = CallTest::kUlpfecPayloadType;
    send_config.rtp.ulpfec.red_rtx_payload_type = CallTest::kRtxRedPayloadType;
  }

  size_t num_streams = config.encoder.num_simulcast_streams;
  encoder_config.number_of_streams = num_streams;
  encoder_config.video_stream_factory =
      new rtc::RefCountedObject<DefaultVideoStreamFactory>();
  encoder_config.max_bitrate_bps = 0;
  encoder_config.simulcast_layers = std::vector<VideoStream>(num_streams);
  if (config.encoder.max_data_rate) {
    encoder_config.max_bitrate_bps = config.encoder.max_data_rate->bps();
  } else {
    for (size_t i = 0; i < num_streams; ++i) {
      encoder_config.max_bitrate_bps +=
          DefaultVideoStreamFactory::kMaxBitratePerStream[i];
    }
  }
  send_stream_ = sender_->call_->CreateVideoSendStream(
      std::move(send_config), std::move(encoder_config));
}

SendVideoStream::~SendVideoStream() {
  sender_->call_->DestroyVideoSendStream(send_stream_);
}

bool SendVideoStream::TrySendPacket(rtc::CopyOnWriteBuffer packet,
                                    uint64_t receiver) {
  sender_->DeliverPacket(MediaType::VIDEO, packet);
  return true;
}
LambdaPrinter SendVideoStream::StatsPrinter() {
  return {"video_target_rate video_sent_rate",
          [this](rtc::SimpleStringBuilder& sb) {
            VideoSendStream::Stats video_stats = send_stream_->GetStats();
            sb.AppendFormat("%.0lf %.0lf",
                            video_stats.target_media_bitrate_bps / 8.0,
                            video_stats.media_bitrate_bps / 8.0);
          },
          64};
}

ReceiveVideoStream::ReceiveVideoStream(CallClient* receiver,
                                       VideoStreamConfig config,
                                       size_t chosen_stream,
                                       Transport* feedback_transport)
    : receiver_(receiver) {
  renderer_ = absl::make_unique<FakeVideoRenderer>();
  auto codec_info = GetCodecInfo(config);
  VideoReceiveStream::Config recv_config(feedback_transport);
  recv_config.rtp.remb = !config.stream.packet_feedback;
  recv_config.rtp.transport_cc = config.stream.packet_feedback;
  recv_config.rtp.local_ssrc = CallTest::kReceiverLocalVideoSsrc;
  recv_config.rtp.extensions = GetVideoRtpExtensions(config);
  recv_config.rtp.nack.rtp_history_ms = config.stream.nack_history_time.ms();
  recv_config.rtp.protected_by_flexfec = config.stream.use_flexfec;
  recv_config.renderer = renderer_.get();
  if (config.stream.num_rtx_streams > chosen_stream) {
    recv_config.rtp.rtx_ssrc = config.stream.rtx_ssrcs[chosen_stream];
    recv_config.rtp
        .rtx_associated_payload_types[CallTest::kSendRtxPayloadType] =
        codec_info.payload_type;
  }
  recv_config.rtp.remote_ssrc = config.stream.ssrcs[chosen_stream];
  VideoReceiveStream::Decoder decoder =
      CreateMatchingDecoder(codec_info.payload_type, codec_info.payload_name);
  decoder_.reset(decoder.decoder);
  recv_config.decoders.push_back(decoder);

  if (config.stream.use_flexfec) {
    RTC_CHECK_EQ(config.encoder.num_simulcast_streams, 1);
    FlexfecReceiveStream::Config flexfec_config(feedback_transport);
    flexfec_config.payload_type = CallTest::kFlexfecPayloadType;
    flexfec_config.remote_ssrc = CallTest::kFlexfecSendSsrc;
    flexfec_config.protected_media_ssrcs = config.stream.ssrcs;
    flexfec_config.local_ssrc = recv_config.rtp.local_ssrc;
    receiver_->call_->CreateFlexfecReceiveStream(flexfec_config);
  }
  if (config.stream.use_ulpfec) {
    recv_config.rtp.red_payload_type = CallTest::kRedPayloadType;
    recv_config.rtp.ulpfec_payload_type = CallTest::kUlpfecPayloadType;
    recv_config.rtp.rtx_associated_payload_types[CallTest::kRtxRedPayloadType] =
        CallTest::kRedPayloadType;
  }
  receiver_->call_->CreateVideoReceiveStream(std::move(recv_config));
}

ReceiveVideoStream::~ReceiveVideoStream() {
  receiver_->call_->DestroyVideoReceiveStream(receive_stream_);
  if (flecfec_stream_)
    receiver_->call_->DestroyFlexfecReceiveStream(flecfec_stream_);
}

bool ReceiveVideoStream::TrySendPacket(rtc::CopyOnWriteBuffer packet,
                                       uint64_t receiver) {
  receiver_->DeliverPacket(MediaType::VIDEO, packet);
  return true;
}
}  // namespace test
}  // namespace webrtc
