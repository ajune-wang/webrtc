/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer3.h"

#include <queue>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"

namespace webrtc {

void FrameBuffer::FrameInfo::SetContinuous() {
  is_continuous_ = true;
}
bool FrameBuffer::FrameInfo::IsContinuous() const {
  return is_continuous_;
}
void FrameBuffer::FrameInfo::AddReverseReference(
    const FrameIterator& reference) {
  referenced_by_.push_back(reference);
}
rtc::ArrayView<const FrameBuffer::FrameIterator>
FrameBuffer::FrameInfo::ReferencedBy() const {
  return referenced_by_;
}
rtc::ArrayView<const int64_t> FrameBuffer::FrameInfo::References() const {
  return {frame_->references, frame_->num_references};
}
void FrameBuffer::FrameInfo::SetEncodedFrame(
    std::unique_ptr<EncodedFrame> frame) {
  frame_ = std::move(frame);
}
bool FrameBuffer::FrameInfo::HasEncodedFrame() const {
  return frame_ != nullptr;
}
bool FrameBuffer::FrameInfo::IsLastSpatialLayer() const {
  return frame_->is_last_spatial_layer;
}
uint32_t FrameBuffer::FrameInfo::RtpTimestamp() const {
  return frame_->Timestamp();
}
std::unique_ptr<EncodedFrame> FrameBuffer::FrameInfo::ExtractEncodedFrame() {
  return std::move(frame_);
}

FrameBuffer::TemporalUnit::TemporalUnit(const FrameIterator& first,
                                        const FrameIterator& last)
    : first_(first), last_(last) {}
uint32_t FrameBuffer::TemporalUnit::RtpTimestamp() const {
  return first_->second.RtpTimestamp();
}
FrameBuffer::FrameIterator FrameBuffer::TemporalUnit::begin() const {
  return first_;
}
FrameBuffer::FrameIterator FrameBuffer::TemporalUnit::end() const {
  return std::next(last_);
}

FrameBuffer::FrameBuffer(int max_frame_slots, int max_decode_history)
    : max_frame_slots_(max_frame_slots),
      decoded_frame_history_(max_decode_history) {}

void FrameBuffer::InsertFrame(std::unique_ptr<EncodedFrame> frame) {
  if (frame->Id() <= decoded_frame_history_.GetLastDecodedFrameId()) {
    // Already decoded past this frame.
    return;
  }

  if (frames_.size() == max_frame_slots_) {
    if (frame->is_keyframe()) {
      Clear();
    } else {
      // No space for this frame.
      return;
    }
  }

  auto frame_it = frames_.emplace(frame->Id(), FrameInfo()).first;
  if (frame_it->second.HasEncodedFrame()) {
    // Frame has already been inserted.
    return;
  }

  frame_it->second.SetEncodedFrame(std::move(frame));
  RegisterReverseReferences(frame_it);
  PropagateContinuity(frame_it);
  // Do we alway have to call this?
  PropagateDecodability(frame_it);
}

absl::optional<int64_t> FrameBuffer::LastContinuousFrameId() const {
  if (last_continuous_frame_) {
    return (*last_continuous_frame_)->first;
  }
  return absl::nullopt;
}

absl::optional<int64_t> FrameBuffer::LastContinuousTemporalUnitFrameId() const {
  if (last_continuous_temporal_unit_) {
    return (*last_continuous_temporal_unit_)->first;
  }
  return absl::nullopt;
}

absl::optional<uint32_t> FrameBuffer::NextDecodableTemporalUnitRtpTimestamp()
    const {
  if (!decodable_temporal_units_.empty()) {
    return static_cast<uint32_t>(decodable_temporal_units_.begin()->first);
  }
  return absl::nullopt;
}

absl::optional<uint32_t> FrameBuffer::LastDecodableTemporalUnitRtpTimestamp()
    const {
  if (!decodable_temporal_units_.empty()) {
    return static_cast<uint32_t>(decodable_temporal_units_.rbegin()->first);
  }
  return absl::nullopt;
}

FrameBuffer::FrameVector FrameBuffer::ExtractNextTemporalUnit() {
  FrameVector res;
  if (decodable_temporal_units_.empty()) {
    return res;
  }

  TemporalUnit& unit = decodable_temporal_units_.begin()->second;
  for (auto it = unit.begin(); it != unit.end(); ++it) {
    decoded_frame_history_.InsertDecoded(it->first, /*timestamp=*/0);
    for (const auto& reverse_reference : it->second.ReferencedBy()) {
      PropagateDecodability(reverse_reference);
    }

    res.push_back(it->second.ExtractEncodedFrame());
  }

  DropNextTemporalUnit();
  return res;
}

void FrameBuffer::DropNextTemporalUnit() {
  if (decodable_temporal_units_.empty()) {
    return;
  }

  TemporalUnit& unit = decodable_temporal_units_.begin()->second;

  frames_.erase(frames_.begin(), unit.end());
  decodable_temporal_units_.erase(decodable_temporal_units_.begin());
}

void FrameBuffer::RegisterReverseReferences(const FrameIterator& frame_it) {
  for (int64_t reference : frame_it->second.References()) {
    // Oftentimes the reference frame has already been decoded, so creating a
    // placeholder for it is not necessary.
    if (reference <= decoded_frame_history_.GetLastDecodedFrameId()) {
      continue;
    }

    // If the reference frame has not yet been inserted then the FrameInfo added
    // to the map will simply be a placeholder. If that frame is later inserted
    // then the placeholder will be populated with the actual frame.
    frames_[reference].AddReverseReference(frame_it);
  }
}

bool FrameBuffer::TestIfContinuous(const FrameIterator& frame_it) const {
  for (int64_t reference : frame_it->second.References()) {
    if (decoded_frame_history_.WasDecoded(reference)) {
      continue;
    }

    auto reference_frame_it = frames_.find(reference);
    if (reference_frame_it != frames_.end() &&
        reference_frame_it->second.IsContinuous()) {
      continue;
    }

    return false;
  }

  return true;
}

void FrameBuffer::PropagateContinuity(const FrameIterator& frame_it) {
  std::queue<FrameIterator> frame_infos;
  frame_infos.push(frame_it);

  while (!frame_infos.empty()) {
    auto& frame_it = frame_infos.front();
    FrameInfo& info = frame_it->second;

    RTC_DCHECK(info.HasEncodedFrame());

    // If this frame is already continuous then continuity has already been
    // propagated along this path, so doing it again won't change anything.
    if (!info.IsContinuous()) {
      if (TestIfContinuous(frame_it)) {
        info.SetContinuous();
        if (info.IsLastSpatialLayer() &&
            (!last_continuous_temporal_unit_ ||
             (*last_continuous_temporal_unit_)->first < frame_it->first)) {
          last_continuous_temporal_unit_ = frame_it;
        }
        if (!last_continuous_frame_ ||
            (*last_continuous_frame_)->first < frame_it->first) {
          last_continuous_frame_ = frame_it;
        }
      }

      // Continue propagation.
      for (const auto& reference : info.ReferencedBy()) {
        frame_infos.push(reference);
      }
    }

    frame_infos.pop();
  }
}

FrameBuffer::TemporalUnit FrameBuffer::FindTemporalUnitForFrame(
    const FrameIterator& frame_it) const {
  RTC_DCHECK(frame_it != frames_.end());

  // Find beginning of temporal unit.
  auto first_it = frame_it;
  while (first_it != frames_.begin()) {
    --first_it;
    if (!first_it->second.HasEncodedFrame() ||
        first_it->second.RtpTimestamp() != frame_it->second.RtpTimestamp()) {
      // Begin iterator is inclusive.
      ++first_it;
      break;
    }
  }

  // Find last_frame of temporal unit.
  auto last_it = frame_it;
  while (last_it != frames_.end()) {
    if (!last_it->second.HasEncodedFrame() ||
        last_it->second.RtpTimestamp() != frame_it->second.RtpTimestamp()) {
      break;
    }
    ++last_it;
  }
  // End iterator is also inclusive!
  --last_it;

  return {first_it, last_it};
}

bool FrameBuffer::IsTemporalUnitCompleteAndDecodable(
    const TemporalUnit& unit) const {
  bool last_frame_is_end_of_temporal_unit = false;
  absl::InlinedVector<int64_t, 4> frames_in_temporal_unit;

  for (auto it = unit.begin(); it != unit.end(); ++it) {
    frames_in_temporal_unit.push_back(it->first);
    for (int64_t reference : it->second.References()) {
      if (!decoded_frame_history_.WasDecoded(reference) &&
          !absl::c_any_of(frames_in_temporal_unit,
                          [reference](int64_t frame_id) {
                            return frame_id == reference;
                          })) {
        // Some undecoded reference was outside the temporal unit, so it is not
        // yet ready to be decoded.
        return false;
      }
    }
    last_frame_is_end_of_temporal_unit = it->second.IsLastSpatialLayer();
  }

  // If the last frame is not the end of the temporal unit then the temporal
  // unit is not complete.
  return last_frame_is_end_of_temporal_unit;
}

void FrameBuffer::PropagateDecodability(const FrameIterator& frame_it) {
  RTC_DCHECK(!decoded_frame_history_.WasDecoded(frame_it->first));
  RTC_DCHECK(frame_it->second.HasEncodedFrame());

  if (decodable_temporal_units_.count(frame_it->second.RtpTimestamp()) > 0) {
    // Frame is already decodable.
    return;
  }

  TemporalUnit unit = FindTemporalUnitForFrame(frame_it);
  if (IsTemporalUnitCompleteAndDecodable(unit)) {
    decodable_temporal_units_.emplace(
        rtp_timestamp_unwrapper_.Unwrap(unit.RtpTimestamp()), std::move(unit));
  }
}

void FrameBuffer::Clear() {
  frames_.clear();
  last_continuous_frame_.reset();
  last_continuous_temporal_unit_.reset();
  decodable_temporal_units_.clear();
  decoded_frame_history_.Clear();
}

}  // namespace webrtc
