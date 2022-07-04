/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SIMULCAST_STREAM_H_
#define API_VIDEO_CODECS_SIMULCAST_STREAM_H_

namespace webrtc {

// TODO(bugs.webrtc.org/6883): Unify with struct VideoStream, part of
// VideoEncoderConfig.
struct SimulcastStream {
  int width = 0;
  int height = 0;
  float maxFramerate = -1;  // fps.
  unsigned char numberOfTemporalLayers = 1;
  unsigned int maxBitrate = -1;     // kilobits/sec.
  unsigned int targetBitrate = -1;  // kilobits/sec.
  unsigned int minBitrate = -1;     // kilobits/sec.
  unsigned int qpMax = -1;          // minimum quality
  bool active = true;               // encoded and sent.
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_SIMULCAST_STREAM_H_
