/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/payload_type_picker.h"

namespace webrtc {

RTCErrorOr<PayloadType> PayloadTypePicker::SuggestMapping(
    cricket::Codec codec) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

RTCError PayloadTypePicker::AddMapping(PayloadType payload_type,
                                       cricket::Codec codec) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

RTCError PayloadTypeRecorder::AddMapping(PayloadType payload_type,
                                         cricket::Codec codec) {
  suggester_.AddMapping(payload_type, codec);
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

std::vector<std::pair<PayloadType, cricket::Codec>>
PayloadTypeRecorder::GetMappings() {
  return std::vector<std::pair<PayloadType, cricket::Codec>>{};
}

RTCErrorOr<PayloadType> PayloadTypeRecorder::LookupPayloadType(
    cricket::Codec codec) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

RTCErrorOr<cricket::Codec> PayloadTypeRecorder::LookupCodec(
    PayloadType payload_type) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

void PayloadTypeRecorder::Checkpoint() {}
void PayloadTypeRecorder::Rollback() {}
void PayloadTypeRecorder::Commit() {}

}  // namespace webrtc
