/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_vp8_encoder.h"

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

namespace {
uint32_t SumStreamMaxBitrate(int streams, const VideoCodec& codec) {
  uint32_t bitrate_sum = 0;
  for (int i = 0; i < streams; ++i) {
    bitrate_sum += codec.simulcastStream[i].maxBitrate;
  }
  return bitrate_sum;
}

int NumberOfStreams(const VideoCodec& codec) {
  int streams =
      codec.numberOfSimulcastStreams < 1 ? 1 : codec.numberOfSimulcastStreams;
  uint32_t simulcast_max_bitrate = SumStreamMaxBitrate(streams, codec);
  if (simulcast_max_bitrate == 0) {
    streams = 1;
  }
  return streams;
}

}  // namespace

namespace test {

FakeVP8Encoder::FakeVP8Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(nullptr) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
  Random random(rtc::TimeMicros());
  picture_id_.reserve(kMaxSimulcastStreams);
  for (int i = 0; i < kMaxSimulcastStreams; ++i) {
    picture_id_.push_back(random.Rand<uint16_t>() & 0x7FFF);
    tl0_pic_idx_.push_back(random.Rand<uint8_t>());
  }
  sequence_checker_.Detach();
}

int32_t FakeVP8Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  callback_ = callback;
  return 0;
}

int32_t FakeVP8Encoder::InitEncode(const VideoCodec* config,
                                   int32_t number_of_cores,
                                   size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  auto result =
      FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    return result;
  }

  int number_of_streams = NumberOfStreams(*config);
  bool doing_simulcast = number_of_streams > 1;

  int num_temporal_layers =
      doing_simulcast ? config->simulcastStream[0].numberOfTemporalLayers
                      : config->VP8().numberOfTemporalLayers;
  RTC_DCHECK_GT(num_temporal_layers, 0);

  SetupTemporalLayers(number_of_streams, num_temporal_layers, *config);

  // timestamp_ = 0;
  // codec_ = *config;

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVP8Encoder::Release() {
  auto result = FakeEncoder::Release();
  sequence_checker_.Detach();
  return result;
}

void FakeVP8Encoder::SetupTemporalLayers(int num_streams,
                                         int num_temporal_layers,
                                         const VideoCodec& codec) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  RTC_DCHECK(codec.VP8().tl_factory != nullptr);
  const TemporalLayersFactory* tl_factory = codec.VP8().tl_factory;
  if (num_streams == 1) {
    temporal_layers_.emplace_back(
        tl_factory->Create(0, num_temporal_layers, tl0_pic_idx_[0]));
  } else {
    for (int i = 0; i < num_streams; ++i) {
      RTC_CHECK_GT(num_temporal_layers, 0);
      int layers = std::max(static_cast<uint8_t>(1),
                            codec.simulcastStream[i].numberOfTemporalLayers);
      temporal_layers_.emplace_back(
          tl_factory->Create(i, layers, tl0_pic_idx_[i]));
    }
  }
}

void FakeVP8Encoder::PopulateCodecSpecific(
    CodecSpecificInfo* codec_specific,
    const TemporalLayers::FrameConfig& tl_config,
    FrameType frame_type,
    int stream_idx,
    uint32_t timestamp) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  codec_specific->codecType = kVideoCodecVP8;
  codec_specific->codec_name = ImplementationName();
  CodecSpecificInfoVP8* vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->pictureId = picture_id_[stream_idx];
  vp8Info->simulcastIdx = stream_idx;
  vp8Info->keyIdx = kNoKeyIdx;
  vp8Info->nonReference = false;  // What should this be set to???   //
                                  // (pkt.data.frame.flags &
                                  // VPX_FRAME_IS_DROPPABLE) != 0;
  temporal_layers_[stream_idx]->PopulateCodecSpecific(
      frame_type == kVideoFrameKey, tl_config, vp8Info, timestamp);
  // Prepare next.
  picture_id_[stream_idx] = (picture_id_[stream_idx] + 1) & 0x7FFF;
  RTC_LOG(LS_INFO) << "stream_idx: " << (int)(stream_idx)
                   << " picture_id: " << vp8Info->pictureId;
}

EncodedImageCallback::Result FakeVP8Encoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragments) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  uint8_t stream_idx = codec_specific_info->codecSpecific.generic.simulcast_idx;
  CodecSpecificInfo overrided_specific_info;
  TemporalLayers::FrameConfig tl_config =
      temporal_layers_[stream_idx]->UpdateLayerConfig(encoded_image._timeStamp);
  PopulateCodecSpecific(&overrided_specific_info, tl_config,
                        encoded_image._frameType, stream_idx,
                        encoded_image._timeStamp);

  return callback_->OnEncodedImage(encoded_image, &overrided_specific_info,
                                   fragments);
}

}  // namespace test
}  // namespace webrtc
