/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VIDEO_JSON_CONFIG_H_
#define TEST_VIDEO_JSON_CONFIG_H_

#include "rtc_base/strings/json.h"
#include "video/video_receive_stream.h"

namespace webrtc {
namespace test {

// Converts a Json representation of the video receive stream configuration into
// a native VideoReceiveStream::Config object. This is shared across both the
// video replayer and the fuzzers to correctly configure scenarios. The goal is
// to make a portable simple way to send rtpdumps and their respective
// configurations around as files to be able to reproduce scenarios.
VideoReceiveStream::Config JsonToVideoReceiveStreamConfig(
    webrtc::Transport* transport,
    const Json::Value& json);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_JSON_CONFIG_H_
