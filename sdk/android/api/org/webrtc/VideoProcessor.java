/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Lightweight abstraction for an object that can receive video frames, process them, and pass them
 * on to another object. This object is also allowed to observe capturer start/stop.
 */
public interface VideoProcessor extends CapturerObserver {
  /** Set the sink that receives the output from this processor. */
  void setSink(VideoSink sink);
}
