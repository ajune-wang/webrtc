/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_FRAME_ANALYZER_VIDEO_ALIGNER_H_
#define RTC_TOOLS_FRAME_ANALYZER_VIDEO_ALIGNER_H_

#include "rtc_tools/y4m_file_reader.h"

namespace webrtc {
namespace test {

// Returns a modified version of the reference video where the frames have been
// aligned to the test video. The test video is assumed to be captured during a
// quality measurement test where the reference video is the source. The test
// video may start at an arbitrary position in the reference video and there
// might be missing frames. The reference video is assumed to loop over when it
// reaches the end. The returned result is a version of the reference video
// where the missing frames are left out so it aligns to the test video.
rtc::scoped_refptr<Video> GenerateAlignedReferenceVideo(
    const rtc::scoped_refptr<Video>& reference_video,
    const rtc::scoped_refptr<Video>& test_video);

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_FRAME_ANALYZER_VIDEO_ALIGNER_H_
