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

namespace webrtc {

VCMDecoderDataBase::~VCMDecoderDataBase() = default;

bool VCMDecoderDataBase::DeregisterExternalDecoder(uint8_t payload_type) {
  auto it = external_decoder_.find(payload_type);
  if (it == external_decoder_.end()) {
    // Not found.
    return false;
  }
  // We can't use payload_type to check if the decoder is currently in use,
  // because payload type may be out of date (e.g. before we decode the first
  // frame after RegisterReceiveCodec).
  if (ptr_decoder_ && ptr_decoder_->IsSameDecoder(it->second)) {
    // Release it if it was registered and in use.
    ptr_decoder_.reset();
  }
  external_decoder_.erase(it);
  return true;
}

// Add the external decoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
void VCMDecoderDataBase::RegisterExternalDecoder(
    uint8_t payload_type,
    VideoDecoder* external_decoder) {
  // If payload value already exists, erase old and insert new.
  external_decoder_[payload_type] = external_decoder;
}

bool VCMDecoderDataBase::IsExternalDecoderRegistered(
    uint8_t payload_type) const {
  return payload_type == current_payload_type_ ||
         external_decoder_.find(payload_type) != external_decoder_.end();
}

bool VCMDecoderDataBase::RegisterReceiveCodec(uint8_t payload_type,
                                              const VideoCodec& receive_codec,
                                              int number_of_cores) {
  if (number_of_cores < 0) {
    return false;
  }
  // If payload value already exists, erase old and insert new.
  auto& entry = decoder_configuration_[payload_type];
  entry.settings = receive_codec;
  entry.number_of_cores = number_of_cores;
  return true;
}

bool VCMDecoderDataBase::DeregisterReceiveCodec(uint8_t payload_type) {
  if (decoder_configuration_.erase(payload_type) == 0) {
    return false;
  }
  if (payload_type == current_payload_type_) {
    // This codec is currently in use.
    current_payload_type_ = absl::nullopt;
  }
  return true;
}

VCMGenericDecoder* VCMDecoderDataBase::GetDecoder(
    const VCMEncodedFrame& frame,
    VCMDecodedFrameCallback* decoded_frame_callback) {
  RTC_DCHECK(decoded_frame_callback);
  RTC_DCHECK(decoded_frame_callback->UserReceiveCallback());

  uint8_t payload_type = frame.PayloadType();
  if (payload_type == current_payload_type_ || payload_type == 0) {
    return ptr_decoder_.get();
  }
  // If decoder exists - delete.
  if (ptr_decoder_) {
    ptr_decoder_ = nullptr;
    current_payload_type_ = absl::nullopt;
  }
  ptr_decoder_ = CreateAndInitDecoder(frame);
  if (!ptr_decoder_) {
    return nullptr;
  }
  current_payload_type_ = payload_type;
  VCMReceiveCallback* callback = decoded_frame_callback->UserReceiveCallback();
  callback->OnIncomingPayloadType(payload_type);
  if (ptr_decoder_->RegisterDecodeCompleteCallback(decoded_frame_callback) <
      0) {
    ptr_decoder_ = nullptr;
    current_payload_type_ = absl::nullopt;
    return nullptr;
  }
  return ptr_decoder_.get();
}

std::unique_ptr<VCMGenericDecoder> VCMDecoderDataBase::CreateAndInitDecoder(
    const VCMEncodedFrame& frame) {
  uint8_t payload_type = frame.PayloadType();
  RTC_LOG(LS_INFO) << "Initializing decoder with payload type '"
                   << int{payload_type} << "'.";
  auto decoder_item = decoder_configuration_.find(payload_type);
  if (decoder_item == decoder_configuration_.end()) {
    RTC_LOG(LS_ERROR) << "Can't find a decoder associated with payload type: "
                      << static_cast<int>(payload_type);
    return nullptr;
  }
  auto external_dec_item = external_decoder_.find(payload_type);
  if (external_dec_item == external_decoder_.end()) {
    RTC_LOG(LS_ERROR) << "No decoder of this type exists.";
    return nullptr;
  }
  auto ptr_decoder =
      std::make_unique<VCMGenericDecoder>(external_dec_item->second);

  // Copy over input resolutions to prevent codec reinitialization due to
  // the first frame being of a different resolution than the database values.
  // This is best effort, since there's no guarantee that width/height have been
  // parsed yet (and may be zero).
  if (frame.EncodedImage()._encodedWidth > 0 &&
      frame.EncodedImage()._encodedHeight > 0) {
    decoder_item->second.settings.width = frame.EncodedImage()._encodedWidth;
    decoder_item->second.settings.height = frame.EncodedImage()._encodedHeight;
  }
  int err = ptr_decoder->InitDecode(&decoder_item->second.settings,
                                    decoder_item->second.number_of_cores);
  if (err < 0) {
    RTC_LOG(LS_ERROR) << "Failed to initialize decoder. Error code: " << err;
    return nullptr;
  }
  return ptr_decoder;
}

}  // namespace webrtc
