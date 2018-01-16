/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_encoder.h"

#include <string.h>

#include <algorithm>
#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {
namespace test {

const int kKeyframeSizeFactor = 10;

FakeEncoder::FakeEncoder(Clock* clock)
    : clock_(clock),
      callback_(nullptr),
      configured_input_framerate_(-1),
      max_target_bitrate_kbps_(-1),
      pending_keyframe_(true),
      debt_bytes_(0) {
  // Generate some arbitrary not-all-zero data
  for (size_t i = 0; i < sizeof(encoded_buffer_); ++i) {
    encoded_buffer_[i] = static_cast<uint8_t>(i);
  }
  memset(used_layers_, 0, sizeof(used_layers_));
}

void FakeEncoder::SetMaxBitrate(int max_kbps) {
  RTC_DCHECK_GE(max_kbps, -1);  // max_kbps == -1 disables it.
  rtc::CritScope cs(&crit_sect_);
  max_target_bitrate_kbps_ = max_kbps;
}

int32_t FakeEncoder::InitEncode(const VideoCodec* config,
                                int32_t number_of_cores,
                                size_t max_payload_size) {
  rtc::CritScope cs(&crit_sect_);
  config_ = *config;
  target_bitrate_.SetBitrate(0, 0, config_.startBitrate * 1000);
  configured_input_framerate_ = config_.maxFramerate;
  pending_keyframe_ = true;
  last_allocation_ = Allocation();
  return 0;
}

int32_t FakeEncoder::Encode(const VideoFrame& input_image,
                            const CodecSpecificInfo* codec_specific_info,
                            const std::vector<FrameType>* frame_types) {
  Allocation allocation = NextFrame(frame_types);
  for (uint8_t i = 0; i < allocation.frame_info.size(); ++i) {
    if (allocation.frame_info[i].size == 0) {
      // Drop this temporal layer.
      continue;
    }

    CodecSpecificInfo specifics;
    memset(&specifics, 0, sizeof(specifics));
    specifics.codecType = kVideoCodecGeneric;
    specifics.codecSpecific.generic.simulcast_idx = i;

    std::unique_ptr<uint8_t[]> encoded_buffer(
        new uint8_t[allocation.frame_info[i].size]);
    memcpy(encoded_buffer.get(), encoded_buffer_,
           allocation.frame_info[i].size);
    EncodedImage encoded(encoded_buffer.get(), allocation.frame_info[i].size,
                         sizeof(encoded_buffer_));
    encoded._timeStamp = input_image.timestamp();
    encoded.capture_time_ms_ = input_image.render_time_ms();
    encoded._frameType =
        allocation.keyframe ? kVideoFrameKey : kVideoFrameDelta;
    encoded._encodedWidth = config_.simulcastStream[i].width;
    encoded._encodedHeight = config_.simulcastStream[i].height;
    encoded.rotation_ = input_image.rotation();
    encoded.content_type_ = (config_.mode == kScreensharing)
                                ? VideoContentType::SCREENSHARE
                                : VideoContentType::UNSPECIFIED;
    specifics.codec_name = ImplementationName();
    if (callback_->OnEncodedImage(encoded, &specifics, nullptr).error !=
        EncodedImageCallback::Result::OK) {
      return -1;
    }
  }
  return 0;
}

FakeEncoder::Allocation FakeEncoder::NextFrame(
    const std::vector<FrameType>* frame_types) {
  Allocation allocation;
  allocation.keyframe = pending_keyframe_;
  pending_keyframe_ = false;

  if (frame_types) {
    for (FrameType frame_type : *frame_types) {
      if (frame_type == kVideoFrameKey) {
        allocation.keyframe = true;
        break;
      }
    }
  }

  for (uint8_t i = 0; i < config_.numberOfSimulcastStreams; ++i) {
    if (target_bitrate_.GetBitrate(i, 0) > 0) {
      int temporal_id =
          last_allocation_.frame_info.size() > i
              ? ++last_allocation_.frame_info[i].temporal_id %
                    config_.simulcastStream[i].numberOfTemporalLayers
              : 0;
      allocation.frame_info.emplace_back(0, temporal_id);
    }
  }

  if (last_allocation_.frame_info.size() < allocation.frame_info.size()) {
    // A new keyframe is needed since a new layer will be added.
    allocation.keyframe = true;
  }

  int framerate = configured_input_framerate_ > 0 ? configured_input_framerate_
                                                  : config_.maxFramerate;
  for (uint8_t i = 0; i < allocation.frame_info.size(); ++i) {
    Allocation::FrameInfo& frame_info = allocation.frame_info[i];
    if (allocation.keyframe) {
      frame_info.temporal_id = 0;
      size_t avg_frame_size =
          (target_bitrate_.GetBitrate(i, 0) + 7) / (8 * framerate);

      // The first frame is a key frame and should be larger.
      // Store the overshoot bytes and distribute them over the coming frames,
      // so that we on average meet the bitrate target.
      debt_bytes_ += (kKeyframeSizeFactor - 1) * avg_frame_size;
      frame_info.size = kKeyframeSizeFactor * avg_frame_size;
    } else {
      size_t avg_frame_size =
          (target_bitrate_.GetBitrate(i, frame_info.temporal_id) + 7) /
          (8 * framerate);
      frame_info.size = avg_frame_size;
      if (debt_bytes_ > 0) {
        // Pay at most half of the frame size for old debts.
        size_t payment_size = std::min(avg_frame_size / 2, debt_bytes_);
        debt_bytes_ -= payment_size;
        frame_info.size -= payment_size;
      }
    }
  }
  last_allocation_ = allocation;
  return allocation;
}

int32_t FakeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope cs(&crit_sect_);
  callback_ = callback;
  return 0;
}

int32_t FakeEncoder::Release() { return 0; }

int32_t FakeEncoder::SetChannelParameters(uint32_t packet_loss, int64_t rtt) {
  return 0;
}

int32_t FakeEncoder::SetRateAllocation(const BitrateAllocation& rate_allocation,
                                       uint32_t framerate) {
  rtc::CritScope cs(&crit_sect_);
  target_bitrate_ = rate_allocation;
  configured_input_framerate_ = framerate;
  return 0;
}

const char* FakeEncoder::kImplementationName = "fake_encoder";
const char* FakeEncoder::ImplementationName() const {
  return kImplementationName;
}

int FakeEncoder::GetConfiguredInputFramerate() const {
  rtc::CritScope cs(&crit_sect_);
  return configured_input_framerate_;
}

FakeH264Encoder::FakeH264Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(nullptr), idr_counter_(0) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
}

int32_t FakeH264Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope cs(&local_crit_sect_);
  callback_ = callback;
  return 0;
}

EncodedImageCallback::Result FakeH264Encoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragments) {
  const size_t kSpsSize = 8;
  const size_t kPpsSize = 11;
  const int kIdrFrequency = 10;
  EncodedImageCallback* callback;
  int current_idr_counter;
  {
    rtc::CritScope cs(&local_crit_sect_);
    callback = callback_;
    current_idr_counter = idr_counter_;
    ++idr_counter_;
  }
  RTPFragmentationHeader fragmentation;
  if (current_idr_counter % kIdrFrequency == 0 &&
      encoded_image._length > kSpsSize + kPpsSize + 1) {
    const size_t kNumSlices = 3;
    fragmentation.VerifyAndAllocateFragmentationHeader(kNumSlices);
    fragmentation.fragmentationOffset[0] = 0;
    fragmentation.fragmentationLength[0] = kSpsSize;
    fragmentation.fragmentationOffset[1] = kSpsSize;
    fragmentation.fragmentationLength[1] = kPpsSize;
    fragmentation.fragmentationOffset[2] = kSpsSize + kPpsSize;
    fragmentation.fragmentationLength[2] =
        encoded_image._length - (kSpsSize + kPpsSize);
    const size_t kSpsNalHeader = 0x67;
    const size_t kPpsNalHeader = 0x68;
    const size_t kIdrNalHeader = 0x65;
    encoded_image._buffer[fragmentation.fragmentationOffset[0]] = kSpsNalHeader;
    encoded_image._buffer[fragmentation.fragmentationOffset[1]] = kPpsNalHeader;
    encoded_image._buffer[fragmentation.fragmentationOffset[2]] = kIdrNalHeader;
  } else {
    const size_t kNumSlices = 1;
    fragmentation.VerifyAndAllocateFragmentationHeader(kNumSlices);
    fragmentation.fragmentationOffset[0] = 0;
    fragmentation.fragmentationLength[0] = encoded_image._length;
    const size_t kNalHeader = 0x41;
    encoded_image._buffer[fragmentation.fragmentationOffset[0]] = kNalHeader;
  }
  uint8_t value = 0;
  int fragment_counter = 0;
  for (size_t i = 0; i < encoded_image._length; ++i) {
    if (fragment_counter == fragmentation.fragmentationVectorSize ||
        i != fragmentation.fragmentationOffset[fragment_counter]) {
      encoded_image._buffer[i] = value++;
    } else {
      ++fragment_counter;
    }
  }
  CodecSpecificInfo specifics;
  memset(&specifics, 0, sizeof(specifics));
  specifics.codecType = kVideoCodecH264;
  specifics.codecSpecific.H264.packetization_mode =
      H264PacketizationMode::NonInterleaved;
  RTC_DCHECK(callback);
  return callback->OnEncodedImage(encoded_image, &specifics, &fragmentation);
}

DelayedEncoder::DelayedEncoder(Clock* clock, int delay_ms)
    : test::FakeEncoder(clock), delay_ms_(delay_ms) {
  // The encoder could be created on a different thread than
  // it is being used on.
  sequence_checker_.Detach();
}

void DelayedEncoder::SetDelay(int delay_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  delay_ms_ = delay_ms;
}

int32_t DelayedEncoder::Encode(const VideoFrame& input_image,
                               const CodecSpecificInfo* codec_specific_info,
                               const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  SleepMs(delay_ms_);

  return FakeEncoder::Encode(input_image, codec_specific_info, frame_types);
}

MultithreadedFakeH264Encoder::MultithreadedFakeH264Encoder(Clock* clock)
    : test::FakeH264Encoder(clock),
      current_queue_(0),
      queue1_(nullptr),
      queue2_(nullptr) {
  // The encoder could be created on a different thread than
  // it is being used on.
  sequence_checker_.Detach();
}

int32_t MultithreadedFakeH264Encoder::InitEncode(const VideoCodec* config,
                                                 int32_t number_of_cores,
                                                 size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  queue1_.reset(new rtc::TaskQueue("Queue 1"));
  queue2_.reset(new rtc::TaskQueue("Queue 2"));

  return FakeH264Encoder::InitEncode(config, number_of_cores, max_payload_size);
}

class MultithreadedFakeH264Encoder::EncodeTask : public rtc::QueuedTask {
 public:
  EncodeTask(MultithreadedFakeH264Encoder* encoder,
             const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types)
      : encoder_(encoder),
        input_image_(input_image),
        codec_specific_info_(),
        frame_types_(*frame_types) {
    if (codec_specific_info)
      codec_specific_info_ = *codec_specific_info;
  }

 private:
  bool Run() override {
    encoder_->EncodeCallback(input_image_, &codec_specific_info_,
                             &frame_types_);
    return true;
  }

  MultithreadedFakeH264Encoder* const encoder_;
  VideoFrame input_image_;
  CodecSpecificInfo codec_specific_info_;
  std::vector<FrameType> frame_types_;
};

int32_t MultithreadedFakeH264Encoder::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  std::unique_ptr<rtc::TaskQueue>& queue =
      (current_queue_++ % 2 == 0) ? queue1_ : queue2_;

  if (!queue) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  queue->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new EncodeTask(this, input_image, codec_specific_info, frame_types)));

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MultithreadedFakeH264Encoder::EncodeCallback(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  return FakeH264Encoder::Encode(input_image, codec_specific_info, frame_types);
}

int32_t MultithreadedFakeH264Encoder::Release() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  queue1_.reset();
  queue2_.reset();

  return FakeH264Encoder::Release();
}

}  // namespace test
}  // namespace webrtc
