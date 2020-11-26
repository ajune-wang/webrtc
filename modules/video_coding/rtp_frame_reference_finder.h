/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
#define MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_

#include <memory>

#include "absl/types/variant.h"
#include "api/array_view.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/rtp_frame_id_only_ref_finder.h"
#include "modules/video_coding/rtp_generic_ref_finder.h"
#include "modules/video_coding/rtp_seq_num_only_ref_finder.h"
#include "modules/video_coding/rtp_vp8_ref_finder.h"
#include "modules/video_coding/rtp_vp9_ref_finder.h"

namespace webrtc {
namespace video_coding {

// A complete frame is a frame which has received all its packets and all its
// references are known.
class OnCompleteFrameCallback {
 public:
  virtual ~OnCompleteFrameCallback() {}
  virtual void OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) = 0;
};

class RtpFrameReferenceFinder {
 public:
  explicit RtpFrameReferenceFinder(OnCompleteFrameCallback* frame_callback);
  explicit RtpFrameReferenceFinder(OnCompleteFrameCallback* frame_callback,
                                   int64_t picture_id_offset);
  ~RtpFrameReferenceFinder();

  // Manage this frame until:
  //  - We have all information needed to determine its references, after
  //    which |frame_callback_| is called with the completed frame, or
  //  - We have too many stashed frames (determined by |kMaxStashedFrames|)
  //    so we drop this frame, or
  //  - It gets cleared by ClearTo, which also means we drop it.
  void ManageFrame(std::unique_ptr<RtpFrameObject> frame);

  // Notifies that padding has been received, which the reference finder
  // might need to calculate the references of a frame.
  void PaddingReceived(uint16_t seq_num);

  // Clear all stashed frames that include packets older than |seq_num|.
  void ClearTo(uint16_t seq_num);

 private:
  using ReturnVector = absl::InlinedVector<std::unique_ptr<RtpFrameObject>, 3>;
  using RefFinder = absl::variant<absl::monostate,
                                  RtpGenericFrameRefFinder,
                                  RtpFrameIdOnlyRefFinder,
                                  RtpSeqNumOnlyRefFinder,
                                  RtpVp8RefFinder,
                                  RtpVp9RefFinder>;

  template <typename T>
  T& SetOrGetRefFinder();
  void HandOffFrames(ReturnVector frames);

  RefFinder ref_finder_;
  int cleared_to_seq_num_ = -1;
  OnCompleteFrameCallback* frame_callback_;
  const int64_t picture_id_offset_;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
