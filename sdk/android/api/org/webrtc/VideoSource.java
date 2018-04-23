/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Java wrapper of native AndroidVideoTrackSource.
 */
@JNINamespace("webrtc::jni")
public class VideoSource extends MediaSource implements VideoCapturer.CapturerObserver {
  public VideoSource(long nativeSource) {
    super(nativeSource);
  }

  /**
   * Calling this function will cause frames to be scaled down to the requested resolution. Also,
   * frames will be cropped to match the requested aspect ratio, and frames will be dropped to match
   * the requested fps. The requested aspect ratio is orientation agnostic and will be adjusted to
   * maintain the input orientation, so it doesn't matter if e.g. 1280x720 or 720x1280 is requested.
   */
  public void adaptOutputFormat(int width, int height, int fps) {
    nativeAdaptOutputFormat(nativeSource, width, height, fps);
  }

  @Override
  public void onCapturerStarted(boolean success) {
    nativeCapturerStarted(nativeSource, success);
  }

  @Override
  public void onCapturerStopped() {
    nativeCapturerStopped(nativeSource);
  }

  @Override
  public void onFrameCaptured(VideoFrame frame) {
    nativeOnFrameCaptured(nativeSource, frame.getBuffer().getWidth(), frame.getBuffer().getHeight(),
        frame.getRotation(), frame.getTimestampNs(), frame.getBuffer());
  }

  private static native void nativeAdaptOutputFormat(long source, int width, int height, int fps);
  private static native void nativeCapturerStarted(long source, boolean success);
  private static native void nativeCapturerStopped(long source);
  private static native void nativeOnFrameCaptured(
      long source, int width, int height, int rotation, long timestampNs, VideoFrame.Buffer frame);
}
