/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr size_t kIrrelatedSimulcastStreamFrameWidth = 2;
constexpr size_t kIrrelatedSimulcastStreamFrameHeight = 2;
constexpr char kIrrelatedSimulcastStreamFrameData[] = "Dummy!";

}  // namespace

rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateDummyFrameBuffer() {
  // Use i420 buffer here as default one and supported by all codecs.
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(kIrrelatedSimulcastStreamFrameWidth,
                                 kIrrelatedSimulcastStreamFrameHeight);
  memcpy(buffer->MutableDataY(), kIrrelatedSimulcastStreamFrameData,
         kIrrelatedSimulcastStreamFrameWidth);
  memcpy(
      buffer->MutableDataY() + buffer->StrideY(),
      kIrrelatedSimulcastStreamFrameData + kIrrelatedSimulcastStreamFrameWidth,
      kIrrelatedSimulcastStreamFrameWidth);
  memcpy(buffer->MutableDataU(),
         kIrrelatedSimulcastStreamFrameData +
             2 * kIrrelatedSimulcastStreamFrameWidth,
         kIrrelatedSimulcastStreamFrameWidth / 2);
  memcpy(buffer->MutableDataV(),
         kIrrelatedSimulcastStreamFrameData +
             5 * kIrrelatedSimulcastStreamFrameWidth / 2,
         kIrrelatedSimulcastStreamFrameWidth / 2);
  return buffer;
}

bool IsDummyFrameBuffer(
    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer) {
  if (buffer->width() != kIrrelatedSimulcastStreamFrameWidth ||
      buffer->height() != kIrrelatedSimulcastStreamFrameHeight) {
    return false;
  }
  if (memcmp(buffer->DataY(), kIrrelatedSimulcastStreamFrameData,
             kIrrelatedSimulcastStreamFrameWidth) != 0) {
    return false;
  }
  if (memcmp(buffer->DataY() + buffer->StrideY(),
             kIrrelatedSimulcastStreamFrameData +
                 kIrrelatedSimulcastStreamFrameWidth,
             kIrrelatedSimulcastStreamFrameWidth) != 0) {
    return false;
  }
  if (memcmp(buffer->DataU(),
             kIrrelatedSimulcastStreamFrameData +
                 2 * kIrrelatedSimulcastStreamFrameWidth,
             kIrrelatedSimulcastStreamFrameWidth / 2) != 0) {
    return false;
  }
  if (memcmp(buffer->DataV(),
             kIrrelatedSimulcastStreamFrameData +
                 5 * kIrrelatedSimulcastStreamFrameWidth / 2,
             kIrrelatedSimulcastStreamFrameWidth / 2) != 0) {
    return false;
  }
  return true;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
