/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/encoder_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"
#include "modules/video_coding/codecs/av1/video_encoder_light.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

EncoderManager::EncoderManager(EncodedImageCallback* encoded_image_signal)
    : encoded_image_signal_(encoded_image_signal) {
  encodings_.resize(1);
  encodings_[0].codec_type = kVideoCodecAV1;
  encodings_[0].encoder = CreateLibaomAv1EncoderLight();
  encodings_[0].structure =
      std::make_unique<ScalableVideoControllerNoLayering>();
}

int32_t EncoderManager::Encode(const VideoFrame& frame,
                               const std::vector<VideoFrameType>* frame_types) {
  RTC_DCHECK(frame_types);
  RTC_DCHECK_LE(frame_types->size(), encodings_.size());
  for (size_t i = 0; i < frame_types->size(); ++i) {
    VideoFrameType type = (*frame_types)[i];
    if (type == VideoFrameType::kEmptyFrame) {
      // Treat as signal for this simulcast layer frame shouldn't be encoding.
      continue;
    }
    bool is_keyframe = (type == VideoFrameType::kVideoFrameKey);
    SimulcastEncoding& encoding = encodings_[i];
    if (!encoding.Enabled()) {
      RTC_LOG(WARNING) << "Odd?";
      continue;
    }
    // TODO(danilchap): callback might happen asynchroniously after this
    // function returns, so do not capture anything that might die by then.
    encoding.encoder->Encode(
        frame, encoding.structure->NextFrameConfig(is_keyframe),
        [&](EncodedFrameLight encoded_frame) {
          // Get encoded image data.
          EncodedImage encoded_image;
          encoded_image.SetEncodedData(std::move(encoded_frame.bitstream));
          encoded_image._frameType = encoded_frame.config.is_keyframe
                                         ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta;
          encoded_image.SetTimestamp(frame.timestamp());
          encoded_image.capture_time_ms_ = frame.render_time_ms();
          encoded_image.rotation_ = frame.rotation();
          encoded_image.content_type_ = VideoContentType::UNSPECIFIED;
          // TODO(danilchap): When spatial scalability is used, set encoded
          // resolution that might be smaller than original frame resolution.
          encoded_image._encodedHeight = frame.height();
          encoded_image._encodedWidth = frame.width();
          encoded_image.timing_.flags = VideoSendTiming::kInvalid;
          encoded_image.qp_ = encoded_frame.qp;
          encoded_image.SetColorSpace(frame.color_space());

          CodecSpecificInfo codec_specific_info;
          codec_specific_info.codecType = encoding.codec_type;
          if (encoded_frame.config.is_keyframe) {
            codec_specific_info.template_structure =
                encoding.structure->DependencyStructure();
          }
          codec_specific_info.generic_frame_info =
              encoding.structure->OnEncodeDone(std::move(encoded_frame.config));
          // TODO(danilchap): Consider running FrameDepndenciesCalculator here.
          encoded_image_signal_->OnEncodedImage(encoded_image,
                                                &codec_specific_info, nullptr);
        });
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace webrtc
