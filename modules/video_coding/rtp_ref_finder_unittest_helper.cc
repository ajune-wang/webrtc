/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/rtp_ref_finder_unittest_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::Matches;
using ::testing::MatchResultListener;
using ::testing::UnorderedElementsAreArray;

namespace webrtc {
namespace {

class HasFrameMatcher
    : public MatcherInterface<
          const std::vector<std::unique_ptr<video_coding::EncodedFrame>>&> {
 public:
  explicit HasFrameMatcher(int64_t frame_id,
                           const std::vector<int64_t>& expected_refs)
      : frame_id_(frame_id), expected_refs_(expected_refs) {}

  bool MatchAndExplain(
      const std::vector<std::unique_ptr<video_coding::EncodedFrame>>& frames,
      MatchResultListener* result_listener) const override {
    auto it = std::find_if(
        frames.begin(), frames.end(),
        [this](const std::unique_ptr<video_coding::EncodedFrame>& f) {
          return f->Id() == frame_id_;
        });
    if (it == frames.end()) {
      if (result_listener->IsInterested()) {
        *result_listener << "No frame with frame_id:" << frame_id_;
      }
      return false;
    }

    rtc::ArrayView<int64_t> actual_refs((*it)->references,
                                        (*it)->num_references);
    if (!Matches(UnorderedElementsAreArray(expected_refs_))(actual_refs)) {
      if (result_listener->IsInterested()) {
        *result_listener << "Frame with frame_id:" << frame_id_ << " and "
                         << actual_refs.size() << " references { ";
        for (auto r : actual_refs) {
          *result_listener << r << " ";
        }
        *result_listener << "}";
      }
      return false;
    }

    return true;
  }

  void DescribeTo(std::ostream* os) const override {  // no-presubmit-check
    *os << "frame with frame_id:" << frame_id_ << " and "
        << expected_refs_.size() << " references { ";
    for (auto r : expected_refs_) {
      *os << r << " ";
    }
    *os << "}";
  }

 private:
  const int64_t frame_id_;
  const std::vector<int64_t> expected_refs_;
};
}  // namespace

// static
Matcher<const std::vector<std::unique_ptr<video_coding::EncodedFrame>>&>
RtpRefFinderTestHelper::HasFrameWithIdAndRefs(int64_t frame_id,
                                              std::vector<int64_t> refs) {
  return MakeMatcher(new HasFrameMatcher(frame_id, refs));
}
}  // namespace webrtc
