/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_FRAME_BUFFER3_H_
#define MODULES_VIDEO_CODING_FRAME_BUFFER3_H_

#include <map>
#include <memory>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "modules/video_coding/utility/decoded_frames_history.h"

namespace webrtc {

class FrameBuffer {
 public:
  using FrameVector = absl::InlinedVector<std::unique_ptr<EncodedFrame>, 4>;

  FrameBuffer(int max_frame_slots, int max_decode_history);
  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  // A temporal unit is a point in time for which one or more frames have been
  // encoded. A temporal unit is considered decodable when all frames in the
  // unit has been received and all references frames has been decoded.
  void InsertFrame(std::unique_ptr<EncodedFrame> frame);
  absl::optional<int64_t> LastContinuousFrameId() const;
  absl::optional<int64_t> LastContinuousTemporalUnitFrameId() const;
  absl::optional<uint32_t> NextDecodableTemporalUnitRtpTimestamp() const;
  absl::optional<uint32_t> LastDecodableTemporalUnitRtpTimestamp() const;
  FrameVector ExtractNextTemporalUnit();
  void DropNextTemporalUnit();

  int GetTotalNumberOfContinuousTemporalUnits() const;
  int GetTotalNumberOfDroppedFrames() const;

 private:
  struct FrameInfo;
  using FrameMap = std::map<int64_t, FrameInfo>;
  using FrameIterator = FrameMap::iterator;

  struct FrameInfo {
    std::unique_ptr<EncodedFrame> encoded_frame;
    bool continuous = false;
  };

  struct TemporalUnit {
    // Both first and last are inclusive.
    FrameIterator first_frame;
    FrameIterator last_frame;
  };

  rtc::ArrayView<const int64_t> GetReferences(const FrameIterator& it) const;
  int64_t GetFrameId(const FrameIterator& it) const;
  int64_t GetTimestamp(const FrameIterator& it) const;
  bool IsLastFrameInTemporalUnit(const FrameIterator& it) const;

  bool TestIfContinuous(const FrameIterator& it) const;
  void PropagateContinuity(const FrameIterator& frame_it);
  void FindNextAndLastDecodableTemporalUnit();
  void Clear();

  const size_t max_frames_;
  FrameMap frames_;
  absl::optional<TemporalUnit> next_decodable_temporal_unit_;
  absl::optional<uint32_t> last_decodable_temporal_unit_timestamp_;
  absl::optional<int64_t> last_continuous_frame_id_;
  absl::optional<int64_t> last_continuous_temporal_unit_frame_id_;
  video_coding::DecodedFramesHistory decoded_frame_history_;
  const bool legacy_frame_id_jump_behavior_;

  int num_continuous_temporal_units_ = 0;
  int num_dropped_frames_ = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_BUFFER3_H_
