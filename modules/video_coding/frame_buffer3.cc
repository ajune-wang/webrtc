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

#include <algorithm>
#include <queue>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"

namespace webrtc {
namespace {
bool ValidReferences(const EncodedFrame& frame) {
  // All references must point backwards, and duplicates are not allowed.
  for (size_t i = 0; i < frame.num_references; ++i) {
    if (frame.references[i] >= frame.Id())
      return false;

    for (size_t j = i + 1; j < frame.num_references; ++j) {
      if (frame.references[i] == frame.references[j])
        return false;
    }
  }

  return true;
}
}  // namespace

rtc::ArrayView<const int64_t> FrameBuffer::GetReferences(
    const FrameIterator& it) const {
  return {it->second.encoded_frame->references,
          std::min<size_t>(it->second.encoded_frame->num_references,
                           EncodedFrame::kMaxFrameReferences)};
}

int64_t FrameBuffer::GetFrameId(const FrameIterator& it) const {
  return it->first;
}

int64_t FrameBuffer::GetTimestamp(const FrameIterator& it) const {
  return it->second.encoded_frame->Timestamp();
}

bool FrameBuffer::IsLastFrameInTemporalUnit(const FrameIterator& it) const {
  return it->second.encoded_frame->is_last_spatial_layer;
}

FrameBuffer::FrameBuffer(int max_frames, int max_decode_history)
    : max_frames_(max_frames), decoded_frame_history_(max_decode_history) {}

void FrameBuffer::InsertFrame(std::unique_ptr<EncodedFrame> frame) {
  if (!ValidReferences(*frame)) {
    return;
  }

  if (frame->Id() <= decoded_frame_history_.GetLastDecodedFrameId()) {
    // Already decoded past this frame.
    return;
  }

  if (frames_.size() == max_frames_) {
    if (frame->is_keyframe()) {
      Clear();
    } else {
      // No space for this frame.
      return;
    }
  }

  auto insert_res = frames_.emplace(frame->Id(), FrameInfo{std::move(frame)});
  if (!insert_res.second) {
    // Frame has already been inserted.
    return;
  }

  PropagateContinuity(insert_res.first);
  FindNextAndLastDecodableTemporalUnit();
}

absl::optional<int64_t> FrameBuffer::LastContinuousFrameId() const {
  return last_continuous_frame_id_;
}

absl::optional<int64_t> FrameBuffer::LastContinuousTemporalUnitFrameId() const {
  return last_continuous_temporal_unit_frame_id_;
}

absl::optional<uint32_t> FrameBuffer::NextDecodableTemporalUnitRtpTimestamp()
    const {
  if (!next_decodable_temporal_unit_) {
    return absl::nullopt;
  }
  return GetTimestamp(next_decodable_temporal_unit_->first_frame);
}

absl::optional<uint32_t> FrameBuffer::LastDecodableTemporalUnitRtpTimestamp()
    const {
  return last_decodable_temporal_unit_timestamp_;
}

FrameBuffer::FrameVector FrameBuffer::ExtractNextTemporalUnit() {
  FrameVector res;
  if (!next_decodable_temporal_unit_) {
    return res;
  }

  auto end_it = std::next(next_decodable_temporal_unit_->last_frame);
  for (auto it = next_decodable_temporal_unit_->first_frame; it != end_it;
       ++it) {
    decoded_frame_history_.InsertDecoded(GetFrameId(it), /*timestamp=*/0);
    res.push_back(std::move(it->second.encoded_frame));
  }

  DropNextTemporalUnit();
  return res;
}

void FrameBuffer::DropNextTemporalUnit() {
  if (!next_decodable_temporal_unit_) {
    return;
  }

  frames_.erase(frames_.begin(),
                std::next(next_decodable_temporal_unit_->last_frame));
  FindNextAndLastDecodableTemporalUnit();
}

bool FrameBuffer::TestIfContinuous(const FrameIterator& it) const {
  for (int64_t reference : GetReferences(it)) {
    if (decoded_frame_history_.WasDecoded(reference)) {
      continue;
    }

    auto reference_frame_it = frames_.find(reference);
    if (reference_frame_it != frames_.end() &&
        reference_frame_it->second.continuous) {
      continue;
    }

    return false;
  }

  return true;
}

void FrameBuffer::PropagateContinuity(const FrameIterator& frame_it) {
  for (auto it = frame_it; it != frames_.end(); ++it) {
    if (!it->second.continuous) {
      if (TestIfContinuous(it)) {
        it->second.continuous = true;
        if (last_continuous_frame_id_ < GetFrameId(it)) {
          last_continuous_frame_id_ = GetFrameId(it);
        }
        if (IsLastFrameInTemporalUnit(it) &&
            last_continuous_temporal_unit_frame_id_ < GetFrameId(it)) {
          last_continuous_temporal_unit_frame_id_ = GetFrameId(it);
        }
      }
    }
  }
}

void FrameBuffer::FindNextAndLastDecodableTemporalUnit() {
  next_decodable_temporal_unit_.reset();
  last_decodable_temporal_unit_timestamp_.reset();

  if (!last_continuous_temporal_unit_frame_id_) {
    return;
  }

  FrameIterator first_frame_it = frames_.begin();
  FrameIterator last_frame_it = frames_.begin();
  absl::InlinedVector<int64_t, 4> frames_in_temporal_unit;
  for (auto frame_it = frames_.begin(); frame_it != frames_.end();) {
    if (GetFrameId(frame_it) > *last_continuous_temporal_unit_frame_id_) {
      break;
    }

    if (GetTimestamp(frame_it) != GetTimestamp(first_frame_it)) {
      frames_in_temporal_unit.clear();
      first_frame_it = frame_it;
    }

    frames_in_temporal_unit.push_back(GetFrameId(frame_it));

    last_frame_it = frame_it++;

    if (IsLastFrameInTemporalUnit(last_frame_it)) {
      bool temporal_unit_decodable = true;
      for (auto it = first_frame_it; it != frame_it && temporal_unit_decodable;
           ++it) {
        for (int64_t reference : GetReferences(it)) {
          if (!decoded_frame_history_.WasDecoded(reference) &&
              !absl::c_any_of(frames_in_temporal_unit,
                              [reference](int64_t frame_id) {
                                return frame_id == reference;
                              })) {
            // Some non-decoded reference was outside the temporal unit, so it's
            // not yet ready to be decoded.
            temporal_unit_decodable = false;
            break;
          }
        }
      }

      if (temporal_unit_decodable) {
        if (!next_decodable_temporal_unit_) {
          next_decodable_temporal_unit_ = {first_frame_it, last_frame_it};
        }

        last_decodable_temporal_unit_timestamp_ = GetTimestamp(first_frame_it);
      }
    }
  }
}

void FrameBuffer::Clear() {
  frames_.clear();
  next_decodable_temporal_unit_.reset();
  last_decodable_temporal_unit_timestamp_.reset();
  last_continuous_frame_id_.reset();
  last_continuous_temporal_unit_frame_id_.reset();
  decoded_frame_history_.Clear();
}

}  // namespace webrtc
