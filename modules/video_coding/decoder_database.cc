/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/decoder_database.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

VCMDecoderDataBase::CurrentDecoderState::CurrentDecoderState(
    uint8_t payload_type,
    VideoDecoder* decoder)
    : payload_type(payload_type), decoder(decoder) {
  RTC_DCHECK(decoder);
}

VCMDecoderDataBase::CurrentDecoderState::~CurrentDecoderState() {
  decoder->Release();
}

VCMDecoderDataBase::VCMDecoderDataBase(VCMDecodedFrameCallback* callback)
    : callback_(callback) {
  RTC_DCHECK(callback_);
}

bool VCMDecoderDataBase::DeregisterExternalDecoder(uint8_t payload_type) {
  auto it = decoders_.find(payload_type);
  if (it == decoders_.end()) {
    // Not found.
    return false;
  }
  // We can't use payload_type to check if the decoder is currently in use,
  // because payload type may be out of date (e.g. before we decode the first
  // frame after RegisterReceiveCodec).
  if (current_ && current_->decoder == it->second) {
    // Release it if it was registered and in use.
    current_ = absl::nullopt;
  }
  decoders_.erase(it);
  return true;
}

// Add the external decoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
void VCMDecoderDataBase::RegisterExternalDecoder(
    uint8_t payload_type,
    VideoDecoder* external_decoder) {
  // If payload value already exists, erase old and insert new.
  DeregisterExternalDecoder(payload_type);
  decoders_[payload_type] = external_decoder;
}

bool VCMDecoderDataBase::IsExternalDecoderRegistered(
    uint8_t payload_type) const {
  return (current_ && payload_type == current_->payload_type) ||
         decoders_.find(payload_type) != decoders_.end();
}

bool VCMDecoderDataBase::RegisterReceiveCodec(uint8_t payload_type,
                                              const VideoCodec& receive_codec,
                                              int number_of_cores) {
  if (number_of_cores < 0) {
    return false;
  }
  // If payload value already exists, erase old and insert new.
  if (current_.has_value() && payload_type == current_->payload_type) {
    current_ = absl::nullopt;
  }
  auto& entry = decoder_settings_[payload_type];
  entry.settings = receive_codec;
  entry.number_of_cores = number_of_cores;
  return true;
}

bool VCMDecoderDataBase::DeregisterReceiveCodec(uint8_t payload_type) {
  if (decoder_settings_.erase(payload_type) == 0) {
    return false;
  }
  if (current_.has_value() && payload_type == current_->payload_type) {
    // This codec is currently in use.
    current_ = absl::nullopt;
  }
  return true;
}

// Decodes frame with decoder registered with `payload_type' equals to
// `frame.payloadType()`
// Reinitializes the decoder when that payload type changes.
// As return value forwards error code returned by `VideoDecoder::Decode`.
int32_t VCMDecoderDataBase::Decode(const VCMEncodedFrame& frame,
                                   Timestamp now) {
  // Change decoder if payload type has changed
  PickDecoder(frame);
  if (!current_.has_value()) {
    return VCM_NO_CODEC_REGISTERED;
  }
  TRACE_EVENT1("webrtc", "VCMDecoderDataBase::Decode", "timestamp",
               frame.Timestamp());
  VCMReceiveCallback* user_callback = callback_->UserReceiveCallback();
  RTC_CHECK(user_callback);

  VCMFrameInformation frame_info;
  frame_info.decodeStart = now;
  frame_info.renderTimeMs = frame.RenderTimeMs();
  frame_info.rotation = frame.rotation();
  frame_info.timing = frame.video_timing();
  frame_info.ntp_time_ms = frame.EncodedImage().ntp_time_ms_;
  frame_info.packet_infos = frame.PacketInfos();

  // Set correctly only for key frames. Thus, use latest key frame
  // content type. If the corresponding key frame was lost, decode will fail
  // and content type will be ignored.
  if (frame.FrameType() == VideoFrameType::kVideoFrameKey) {
    frame_info.content_type = frame.contentType();
    current_->content_type = frame.contentType();
  } else {
    frame_info.content_type = current_->content_type;
  }
  callback_->Map(frame.Timestamp(), frame_info);

  int32_t ret = current_->decoder->Decode(
      frame.EncodedImage(), frame.MissingFrame(), frame.RenderTimeMs());
  VideoDecoder::DecoderInfo decoder_info = current_->decoder->GetDecoderInfo();
  if (decoder_info != current_->decoder_info) {
    RTC_LOG(LS_INFO) << "Changed decoder implementation to: "
                     << decoder_info.ToString();
    current_->decoder_info = decoder_info;
    user_callback->OnDecoderImplementationName(
        decoder_info.implementation_name.empty()
            ? "unknown"
            : decoder_info.implementation_name.c_str());
  }
  if (ret < WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Failed to decode frame with timestamp "
                        << frame.Timestamp() << ", error code: " << ret;
    callback_->ClearTimestampMap();
  } else if (ret == WEBRTC_VIDEO_CODEC_NO_OUTPUT) {
    // No output.
    callback_->ClearTimestampMap();
  }
  return ret;
}

void VCMDecoderDataBase::PickDecoder(const VCMEncodedFrame& frame) {
  uint8_t payload_type = frame.PayloadType();
  if (payload_type == 0) {
    return;
  }
  if (current_.has_value() && payload_type == current_->payload_type) {
    return;
  }
  // If decoder exists - delete.
  current_ = absl::nullopt;
  RTC_LOG(LS_INFO) << "Initializing decoder with payload type '"
                   << int{payload_type} << "'.";
  auto decoder_item = decoder_settings_.find(payload_type);
  if (decoder_item == decoder_settings_.end()) {
    RTC_LOG(LS_ERROR) << "Can't find a decoder associated with payload type: "
                      << int{payload_type};
    return;
  }
  auto external_dec_item = decoders_.find(payload_type);
  if (external_dec_item == decoders_.end()) {
    RTC_LOG(LS_ERROR) << "No decoder of this type exists.";
    return;
  }
  current_.emplace(payload_type, external_dec_item->second);

  // Copy over input resolutions to prevent codec reinitialization due to
  // the first frame being of a different resolution than the database values.
  // This is best effort, since there's no guarantee that width/height have been
  // parsed yet (and may be zero).
  if (frame.EncodedImage()._encodedWidth > 0 &&
      frame.EncodedImage()._encodedHeight > 0) {
    decoder_item->second.settings.width = frame.EncodedImage()._encodedWidth;
    decoder_item->second.settings.height = frame.EncodedImage()._encodedHeight;
  }
  int err = current_->decoder->InitDecode(&decoder_item->second.settings,
                                          decoder_item->second.number_of_cores);
  if (err < 0) {
    current_ = absl::nullopt;
    RTC_LOG(LS_ERROR) << "Failed to initialize decoder. Error code: " << err;
    return;
  }

  current_->decoder_info = current_->decoder->GetDecoderInfo();
  RTC_LOG(LS_INFO) << "Decoder implementation: "
                   << current_->decoder_info.ToString();

  VCMReceiveCallback* receive_callback = callback_->UserReceiveCallback();
  receive_callback->OnIncomingPayloadType(payload_type);
  if (current_->decoder->RegisterDecodeCompleteCallback(callback_) < 0) {
    current_ = absl::nullopt;
  }
  if (!current_->decoder_info.implementation_name.empty()) {
    receive_callback->OnDecoderImplementationName(
        current_->decoder_info.implementation_name.c_str());
  }
}

}  // namespace webrtc
