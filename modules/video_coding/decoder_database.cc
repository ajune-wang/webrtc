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

VCMExtDecoderMapItem::VCMExtDecoderMapItem(
    VideoDecoder* external_decoder_instance,
    uint8_t payload_type)
    : payload_type(payload_type),
      external_decoder_instance(external_decoder_instance) {}

VCMDecoderDataBase::~VCMDecoderDataBase() {
  ptr_decoder_.reset();
  for (auto& kv : dec_external_map_)
    delete kv.second;
}

bool VCMDecoderDataBase::DeregisterExternalDecoder(uint8_t payload_type) {
  ExternalDecoderMap::iterator it = dec_external_map_.find(payload_type);
  if (it == dec_external_map_.end()) {
    // Not found.
    return false;
  }
  // We can't use payload_type to check if the decoder is currently in use,
  // because payload type may be out of date (e.g. before we decode the first
  // frame after RegisterReceiveCodec).
  if (ptr_decoder_ &&
      ptr_decoder_->IsSameDecoder((*it).second->external_decoder_instance)) {
    // Release it if it was registered and in use.
    ptr_decoder_.reset();
  }
  delete it->second;
  dec_external_map_.erase(it);
  return true;
}

// Add the external decoder object to the list of external decoders.
// Won't be registered as a receive codec until RegisterReceiveCodec is called.
void VCMDecoderDataBase::RegisterExternalDecoder(VideoDecoder* external_decoder,
                                                 uint8_t payload_type) {
  // If payload value already exists, erase old and insert new.
  VCMExtDecoderMapItem* ext_decoder =
      new VCMExtDecoderMapItem(external_decoder, payload_type);
  DeregisterExternalDecoder(payload_type);
  dec_external_map_[payload_type] = ext_decoder;
}

bool VCMDecoderDataBase::IsExternalDecoderRegistered(
    uint8_t payload_type) const {
  return payload_type == current_payload_type_ ||
         FindExternalDecoderItem(payload_type);
}

bool VCMDecoderDataBase::RegisterReceiveCodec(
    uint8_t payload_type,
    const VideoDecoder::Config& decoder_config) {
  if (decoder_config.number_of_cores() < 0) {
    return false;
  }
  // If payload value already exists, erase old and insert new.
  dec_map_[payload_type] = decoder_config;
  return true;
}

bool VCMDecoderDataBase::DeregisterReceiveCodec(uint8_t payload_type) {
  if (dec_map_.erase(payload_type) == 0) {
    return false;
  }
  if (payload_type == current_payload_type_) {
    // This codec is currently in use.
    current_payload_type_ = 0;
  }
  return true;
}

VCMGenericDecoder* VCMDecoderDataBase::GetDecoder(
    const VCMEncodedFrame& frame,
    VCMDecodedFrameCallback* decoded_frame_callback) {
  RTC_DCHECK(decoded_frame_callback->UserReceiveCallback());
  uint8_t payload_type = frame.PayloadType();
  if (payload_type == current_payload_type_ || payload_type == 0) {
    return ptr_decoder_.get();
  }
  // If decoder exists - delete.
  if (ptr_decoder_) {
    ptr_decoder_.reset();
    current_payload_type_ = 0;
  }
  ptr_decoder_ = CreateAndInitDecoder(frame);
  if (!ptr_decoder_) {
    return nullptr;
  }
  current_payload_type_ = frame.PayloadType();
  VCMReceiveCallback* callback = decoded_frame_callback->UserReceiveCallback();
  callback->OnIncomingPayloadType(current_payload_type_);
  if (ptr_decoder_->RegisterDecodeCompleteCallback(decoded_frame_callback) <
      0) {
    ptr_decoder_.reset();
    current_payload_type_ = 0;
    return nullptr;
  }
  return ptr_decoder_.get();
}

std::unique_ptr<VCMGenericDecoder> VCMDecoderDataBase::CreateAndInitDecoder(
    const VCMEncodedFrame& frame) {
  uint8_t payload_type = frame.PayloadType();
  RTC_LOG(LS_INFO) << "Initializing decoder with payload type '"
                   << static_cast<int>(payload_type) << "'.";
  auto decoder_config_it = dec_map_.find(payload_type);
  if (decoder_config_it == dec_map_.end()) {
    RTC_LOG(LS_ERROR) << "Can't find a decoder associated with payload type: "
                      << int{payload_type};
    return nullptr;
  }
  std::unique_ptr<VCMGenericDecoder> ptr_decoder;
  const VCMExtDecoderMapItem* external_dec_item =
      FindExternalDecoderItem(payload_type);
  if (external_dec_item) {
    // External codec.
    ptr_decoder.reset(new VCMGenericDecoder(
        external_dec_item->external_decoder_instance, true));
  } else {
    RTC_LOG(LS_ERROR) << "No decoder of this type exists.";
  }
  if (!ptr_decoder)
    return nullptr;

  // Copy over input resolutions to prevent codec reinitialization due to
  // the first frame being of a different resolution than the database values.
  // This is best effort, since there's no guarantee that width/height have been
  // parsed yet (and may be zero).
  if (frame.EncodedImage()._encodedWidth > 0 &&
      frame.EncodedImage()._encodedHeight > 0) {
    decoder_config_it->second.set_max_encoded_resolution(
        {static_cast<int>(frame.EncodedImage()._encodedWidth),
         static_cast<int>(frame.EncodedImage()._encodedHeight)});
  }
  if (!ptr_decoder->Init(decoder_config_it->second)) {
    RTC_LOG(LS_ERROR) << "Failed to initialize decoder.";
    return nullptr;
  }
  return ptr_decoder;
}

const VCMExtDecoderMapItem* VCMDecoderDataBase::FindExternalDecoderItem(
    uint8_t payload_type) const {
  ExternalDecoderMap::const_iterator it = dec_external_map_.find(payload_type);
  if (it != dec_external_map_.end()) {
    return (*it).second;
  }
  return nullptr;
}

}  // namespace webrtc
