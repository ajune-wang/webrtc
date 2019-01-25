/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.support.annotation.Nullable;
import org.webrtc.VideoFrame;

/**
 * Implements VideoCapturer.CapturerObserver and feeds frames to
 * webrtc::jni::AndroidVideoTrackSource.
 */
class NativeCapturerObserver implements CapturerObserver {
  private static class FrameAdaptationParameters {
    int cropX;
    int cropY;
    int cropWidth;
    int cropHeight;
    int scaleWidth;
    int scaleHeight;
    long timestampNs;

    @CalledByNative("FrameAdaptationParameters")
    FrameAdaptationParameters(int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth,
        int scaleHeight, long timestampNs) {
      this.cropX = cropX;
      this.cropY = cropY;
      this.cropWidth = cropWidth;
      this.cropHeight = cropHeight;
      this.scaleWidth = scaleWidth;
      this.scaleHeight = scaleHeight;
      this.timestampNs = timestampNs;
    }
  }

  // Pointer to webrtc::jni::AndroidVideoTrackSource.
  private final long nativeSource;
  @Nullable private volatile VideoProcessor videoProcessor;

  /**
   * Hook for injecting a custom video processor before frames are passed onto WebRTC. The frames
   * will be cropped and scaled depending on CPU and network conditions before they are passed to
   * the video processor.
   */
  public void setVideoProcessor(@Nullable VideoProcessor videoProcessor) {
    this.videoProcessor = videoProcessor;
    if (videoProcessor != null) {
      videoProcessor.setSink(this::deliverFrameToWebRtc);
    }
  }

  @CalledByNative
  public NativeCapturerObserver(long nativeSource) {
    this.nativeSource = nativeSource;
  }

  @Override
  public void onCapturerStarted(boolean success) {
    nativeCapturerStarted(nativeSource, success);
    final VideoProcessor videoProcessor = this.videoProcessor;
    if (videoProcessor != null) {
      videoProcessor.onCapturerStarted(success);
    }
  }

  @Override
  public void onCapturerStopped() {
    nativeCapturerStopped(nativeSource);
    final VideoProcessor videoProcessor = this.videoProcessor;
    if (videoProcessor != null) {
      videoProcessor.onCapturerStopped();
    }
  }

  @Override
  public void onFrameCaptured(VideoFrame frame) {
    @Nullable
    final FrameAdaptationParameters parameters =
        nativeGetFrameAdaptationParameters(nativeSource, frame.getBuffer().getWidth(),
            frame.getBuffer().getHeight(), frame.getRotation(), frame.getTimestampNs());
    if (parameters == null) {
      // Drop frame.
      return;
    }
    final VideoFrame.Buffer adaptedBuffer =
        frame.getBuffer().cropAndScale(parameters.cropX, parameters.cropY, parameters.cropWidth,
            parameters.cropHeight, parameters.scaleWidth, parameters.scaleHeight);
    final VideoFrame adaptedFrame =
        new VideoFrame(adaptedBuffer, frame.getRotation(), parameters.timestampNs);

    final VideoProcessor videoProcessor = this.videoProcessor;
    if (videoProcessor != null) {
      videoProcessor.onFrameCaptured(adaptedFrame);
    } else {
      deliverFrameToWebRtc(adaptedFrame);
    }

    adaptedFrame.release();
  }

  private void deliverFrameToWebRtc(VideoFrame frame) {
    nativeOnFrameCaptured(
        nativeSource, frame.getRotation(), frame.getTimestampNs(), frame.getBuffer());
  }

  private static native void nativeCapturerStarted(long source, boolean success);
  private static native void nativeCapturerStopped(long source);
  @Nullable
  private static native FrameAdaptationParameters nativeGetFrameAdaptationParameters(
      long source, int width, int height, int rotation, long timestampNs);
  private static native void nativeOnFrameCaptured(
      long source, int rotation, long timestampNs, VideoFrame.Buffer frame);
}
