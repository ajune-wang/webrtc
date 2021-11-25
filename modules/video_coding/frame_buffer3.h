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
#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {

class FrameBuffer {
 public:
  using FrameVector = absl::InlinedVector<std::unique_ptr<EncodedFrame>, 3>;

  FrameBuffer(int max_frame_slots, int max_decode_history);
  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  void InsertFrame(std::unique_ptr<EncodedFrame> frame);
  absl::optional<int64_t> LastContinuousFrameId() const;
  absl::optional<int64_t> LastContinuousTemporalUnitFrameId() const;
  absl::optional<uint32_t> NextDecodableTemporalUnitRtpTimestamp() const;
  absl::optional<uint32_t> LastDecodableTemporalUnitRtpTimestamp() const;
  FrameVector ExtractNextTemporalUnit();
  void DropNextTemporalUnit();

 private:
  class FrameInfo;
  using FrameIterator = std::map<int64_t, FrameInfo>::iterator;

  class FrameInfo {
   public:
    void SetContinuous();
    bool IsContinuous() const;

    void AddReverseReference(const FrameIterator& reference);
    rtc::ArrayView<const FrameIterator> ReferencedBy() const;
    rtc::ArrayView<const int64_t> References() const;

    void SetEncodedFrame(std::unique_ptr<EncodedFrame> frame);
    bool HasEncodedFrame() const;
    bool IsLastSpatialLayer() const;
    uint32_t RtpTimestamp() const;
    std::unique_ptr<EncodedFrame> ExtractEncodedFrame();

   private:
    bool is_continuous_ = false;
    std::unique_ptr<EncodedFrame> frame_;
    absl::InlinedVector<FrameIterator, 4> referenced_by_;
  };

  class TemporalUnit {
   public:
    TemporalUnit(const FrameIterator& first, const FrameIterator& last);
    TemporalUnit(const TemporalUnit&) = default;
    TemporalUnit& operator=(const TemporalUnit&) = default;
    uint32_t RtpTimestamp() const;
    FrameIterator begin() const;
    FrameIterator end() const;

   private:
    // First and last iterators are inclusive.
    FrameIterator first_;
    FrameIterator last_;
  };

  void RegisterReverseReferences(const FrameIterator& frame_it);
  bool TestIfContinuous(const FrameIterator& frame_it) const;
  void PropagateContinuity(const FrameIterator& frame_it);
  TemporalUnit FindTemporalUnitForFrame(const FrameIterator& frame_it) const;
  bool IsTemporalUnitCompleteAndDecodable(
      const TemporalUnit& temporal_unit) const;
  void PropagateDecodability(const FrameIterator& frame_it);
  void Clear();

  const size_t max_frame_slots_;
  // Map frame id --> frame info
  std::map<int64_t, FrameInfo> frames_;
  // Map unwrapped rtp timestamp --> temporal unit.
  std::map<int64_t, TemporalUnit> decodable_temporal_units_;
  // Points to the last continuous frame.
  absl::optional<FrameIterator> last_continuous_frame_;
  // Points to the last frame in the last continuous temporal unit.
  absl::optional<FrameIterator> last_continuous_temporal_unit_;
  video_coding::DecodedFramesHistory decoded_frame_history_;
  SeqNumUnwrapper<uint32_t> rtp_timestamp_unwrapper_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_FRAME_BUFFER3_H_
