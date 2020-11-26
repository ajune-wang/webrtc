/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_frame_reference_finder.h"

#include <utility>

#include "modules/video_coding/frame_object.h"

namespace webrtc {
namespace video_coding {

RtpFrameReferenceFinder::RtpFrameReferenceFinder(
    OnCompleteFrameCallback* frame_callback)
    : RtpFrameReferenceFinder(frame_callback, 0) {}

RtpFrameReferenceFinder::RtpFrameReferenceFinder(
    OnCompleteFrameCallback* frame_callback,
    int64_t picture_id_offset)
    : frame_callback_(frame_callback), picture_id_offset_(picture_id_offset) {}

RtpFrameReferenceFinder::~RtpFrameReferenceFinder() = default;

void RtpFrameReferenceFinder::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  // If we have cleared past this frame, drop it.
  if (cleared_to_seq_num_ != -1 &&
      AheadOf<uint16_t>(cleared_to_seq_num_, frame->first_seq_num())) {
    return;
  }

  const RTPVideoHeader& video_header = frame->GetRtpVideoHeader();

  if (video_header.generic.has_value()) {
    HandOffFrames(SetOrGetRefFinder<RtpGenericFrameRefFinder>().ManageFrame(
        std::move(frame), *video_header.generic));
    return;
  }

  switch (frame->codec_type()) {
    case kVideoCodecVP8: {
      const RTPVideoHeaderVP8& vp8_header =
          absl::get<RTPVideoHeaderVP8>(video_header.video_type_header);

      if (vp8_header.temporalIdx == kNoTemporalIdx ||
          vp8_header.tl0PicIdx == kNoTl0PicIdx) {
        if (vp8_header.pictureId == kNoPictureId) {
          HandOffFrames(SetOrGetRefFinder<RtpSeqNumOnlyRefFinder>().ManageFrame(
              std::move(frame)));
          return;
        }

        HandOffFrames(SetOrGetRefFinder<RtpFrameIdOnlyRefFinder>().ManageFrame(
            std::move(frame), vp8_header.pictureId));
        return;
      }

      HandOffFrames(
          SetOrGetRefFinder<RtpVp8RefFinder>().ManageFrame(std::move(frame)));
      return;
    }
    case kVideoCodecVP9: {
      const RTPVideoHeaderVP9& vp9_header =
          absl::get<RTPVideoHeaderVP9>(video_header.video_type_header);

      if (vp9_header.temporal_idx == kNoTemporalIdx) {
        if (vp9_header.picture_id == kNoPictureId) {
          HandOffFrames(SetOrGetRefFinder<RtpSeqNumOnlyRefFinder>().ManageFrame(
              std::move(frame)));
          return;
        }

        HandOffFrames(SetOrGetRefFinder<RtpFrameIdOnlyRefFinder>().ManageFrame(
            std::move(frame), vp9_header.picture_id));
        return;
      }

      HandOffFrames(
          SetOrGetRefFinder<RtpVp9RefFinder>().ManageFrame(std::move(frame)));
      return;
    }
    case kVideoCodecH264: {
      HandOffFrames(SetOrGetRefFinder<RtpSeqNumOnlyRefFinder>().ManageFrame(
          std::move(frame)));
      return;
    }
    case kVideoCodecGeneric: {
      if (auto* generic_header = absl::get_if<RTPVideoHeaderLegacyGeneric>(
              &video_header.video_type_header)) {
        HandOffFrames(SetOrGetRefFinder<RtpFrameIdOnlyRefFinder>().ManageFrame(
            std::move(frame), generic_header->picture_id));
        return;
      }

      HandOffFrames(SetOrGetRefFinder<RtpSeqNumOnlyRefFinder>().ManageFrame(
          std::move(frame)));
      return;
    }
    default:
      RTC_NOTREACHED();
  }
}

template <typename T>
T& RtpFrameReferenceFinder::SetOrGetRefFinder() {
  if (auto* ref_finder = absl::get_if<T>(&ref_finder_)) {
    return *ref_finder;
  }
  return ref_finder_.emplace<T>();
}

void RtpFrameReferenceFinder::HandOffFrames(ReturnVector frames) {
  for (auto& frame : frames) {
    frame->id.picture_id += picture_id_offset_;
    for (size_t i = 0; i < frame->num_references; ++i) {
      frame->references[i] += picture_id_offset_;
    }

    frame_callback_->OnCompleteFrame(std::move(frame));
  }
}

void RtpFrameReferenceFinder::PaddingReceived(uint16_t seq_num) {
  if (auto* ref_finder = absl::get_if<RtpSeqNumOnlyRefFinder>(&ref_finder_)) {
    HandOffFrames(ref_finder->PaddingReceived(seq_num));
  }
}

void RtpFrameReferenceFinder::ClearTo(uint16_t seq_num) {
  cleared_to_seq_num_ = seq_num;

  struct ClearToVisitor {
    void operator()(absl::monostate& ref_finder) {}
    void operator()(RtpGenericFrameRefFinder& ref_finder) {}
    void operator()(RtpFrameIdOnlyRefFinder& ref_finder) {}
    void operator()(RtpSeqNumOnlyRefFinder& ref_finder) {
      ref_finder.ClearTo(seq_num);
    }
    void operator()(RtpVp8RefFinder& ref_finder) {
      ref_finder.ClearTo(seq_num);
    }
    void operator()(RtpVp9RefFinder& ref_finder) {
      ref_finder.ClearTo(seq_num);
    }
    uint16_t seq_num;
  };

  absl::visit(ClearToVisitor{seq_num}, ref_finder_);
}

}  // namespace video_coding
}  // namespace webrtc
