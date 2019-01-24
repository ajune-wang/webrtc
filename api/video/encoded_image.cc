/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/encoded_image.h"

#include <string.h>

namespace webrtc {

EncodedImage::EncodedImage() : EncodedImage(nullptr, 0, 0) {}

EncodedImage::EncodedImage(const EncodedImage&) = default;

EncodedImage::EncodedImage(uint8_t* buffer, size_t size, size_t capacity)
    : buffer_(buffer), size_(size), capacity_(capacity) {}

void EncodedImage::SetEncodeTime(int64_t encode_start_ms,
                                 int64_t encode_finish_ms) {
  timing_.encode_start_ms = encode_start_ms;
  timing_.encode_finish_ms = encode_finish_ms;
}
}  // namespace webrtc
