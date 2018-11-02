/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_COMMON_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_COMMON_H_

#include <stdint.h>

namespace webrtc {

// Convert between the packet fraction loss - a floating point number in
// the range [0.0, 1.0] - and a uint32_t with up to a fixed number of bits,
// which can be efficiently stored in a protobuf and delta-encoded.
uint32_t ConvertPacketLossFractionToProtoFormat(float packet_loss_fraction);
bool ParsePacketLossFractionFromProtoFormat(uint32_t proto_packet_loss_fraction,
                                            float* output);

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_COMMON_H_
