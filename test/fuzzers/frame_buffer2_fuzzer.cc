/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer2.h"

#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {
template <typename T>
T ReadNum(const uint8_t* data, size_t* offset, size_t max_size) {
  RTC_CHECK(*offset + sizeof(T) < max_size);
  T res = *reinterpret_cast<const T*>(data + *offset);
  *offset += sizeof(T);
  return res;
}
}  // namespace

class FuzzyFrameObject : public video_coding::FrameObject {
 public:
  FuzzyFrameObject() {}
  ~FuzzyFrameObject() {}

  bool GetBitstream(uint8_t* destination) const override { return false; }
  uint32_t Timestamp() const override { return timestamp; }
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return _renderTimeMs; }
};

void FuzzOneInput(const uint8_t* data, size_t size) {
  Clock* clock = Clock::GetRealTimeClock();
  VCMJitterEstimator jitter_estimator(clock, 0, 0);
  VCMTiming timing(clock);
  video_coding::FrameBuffer frame_buffer(clock, &jitter_estimator, &timing,
                                         nullptr);

  size_t offset = 0;
  while (true) {
    if (offset + 1 >= size)
      return;

    if (ReadNum<uint8_t>(data, &offset, size) & 1) {
      if (offset + 14 >= size)
        return;

      std::unique_ptr<FuzzyFrameObject> frame(new FuzzyFrameObject());
      frame->picture_id = ReadNum<int64_t>(data, &offset, size);
      frame->spatial_layer = ReadNum<uint8_t>(data, &offset, size) & 7;
      frame->timestamp = ReadNum<uint32_t>(data, &offset, size);
      frame->num_references = ReadNum<uint8_t>(data, &offset, size) % 6;

      if (offset + frame->num_references * 8 >= size)
        return;
      for (size_t r = 0; r < frame->num_references; ++r)
        frame->references[r] = ReadNum<int64_t>(data, &offset, size);

      frame_buffer.InsertFrame(std::move(frame));

    } else {
      if (offset + 1 >= size)
        return;

      int64_t wait_ms = ReadNum<uint8_t>(data, &offset, size) & 7;
      std::unique_ptr<video_coding::FrameObject> frame(new FuzzyFrameObject());

      frame_buffer.NextFrame(wait_ms, &frame);
    }
  }
}

}  // namespace webrtc
