/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.I420ABuffer;

/** Implementation of VideoFrame.I420Buffer backed by Java direct byte buffers. */
public class JavaI420ABuffer implements VideoFrame.I420ABuffer {
  private final JavaI420Buffer yuvImage;
  private final ByteBuffer dataA;
  private final int strideA;

  private final Runnable releaseCallback;
  private final Object refCountLock = new Object();
  private int refCount;

  private JavaI420ABuffer(int width, int height, ByteBuffer dataY, int strideY, ByteBuffer dataU,
      int strideU, ByteBuffer dataV, int strideV, ByteBuffer dataA, int strideA,
      Runnable releaseCallback) {
    yuvImage =
        JavaI420Buffer.wrap(width, height, dataY, strideY, dataU, strideU, dataV, strideV, null);
    this.dataA = dataA;
    this.strideA = strideA;

    this.releaseCallback = releaseCallback;
    this.refCount = 1;
  }

  /** Wraps existing ByteBuffers into JavaI420Buffer object without copying the contents. */
  public static JavaI420ABuffer wrap(int width, int height, ByteBuffer dataY, int strideY,
      ByteBuffer dataU, int strideU, ByteBuffer dataV, int strideV, ByteBuffer dataA, int strideA,
      Runnable releaseCallback) {
    if (dataY == null || dataU == null || dataV == null || dataA == null) {
      throw new IllegalArgumentException("Data buffers cannot be null.");
    }
    if (!dataY.isDirect() || !dataU.isDirect() || !dataV.isDirect() || !dataA.isDirect()) {
      throw new IllegalArgumentException("Data buffers must be direct byte buffers.");
    }

    // Slice the buffers to prevent external modifications to the position / limit of the buffer.
    // Note that this doesn't protect the contents of the buffers from modifications.
    dataA = dataA.slice();

    final int minCapacityA = strideA * height;
    if (dataA.capacity() < minCapacityA) {
      throw new IllegalArgumentException("A-buffer must be at least " + minCapacityA + " bytes.");
    }

    return new JavaI420ABuffer(width, height, dataY, strideY, dataU, strideU, dataV, strideV, dataA,
        strideA, releaseCallback);
  }

  /** Allocates an empty I420Buffer suitable for an image of the given dimensions. */
  public static JavaI420ABuffer allocate(int width, int height) {
    int chromaHeight = (height + 1) / 2;
    int strideUV = (width + 1) / 2;
    int yPos = 0;
    int uPos = yPos + width * height;
    int vPos = uPos + strideUV * chromaHeight;
    int aPos = vPos + strideUV * chromaHeight;

    ByteBuffer buffer = ByteBuffer.allocateDirect(2 * width * height + 2 * strideUV * chromaHeight);

    buffer.position(yPos);
    buffer.limit(uPos);
    ByteBuffer dataY = buffer.slice();

    buffer.position(uPos);
    buffer.limit(vPos);
    ByteBuffer dataU = buffer.slice();

    buffer.position(vPos);
    buffer.limit(aPos);
    ByteBuffer dataV = buffer.slice();

    buffer.position(aPos);
    buffer.limit(aPos + width * height);
    ByteBuffer dataA = buffer.slice();

    return new JavaI420ABuffer(width, height, dataY, width, dataU, strideUV, dataV, strideUV, dataA,
        width, null /* releaseCallback */);
  }

  @Override
  public int getWidth() {
    return yuvImage.getWidth();
  }

  @Override
  public int getHeight() {
    return yuvImage.getHeight();
  }

  @Override
  public int getStrideY() {
    return yuvImage.getStrideY();
  }

  @Override
  public ByteBuffer getDataY() {
    return yuvImage.getDataY();
  }

  @Override
  public int getStrideU() {
    return yuvImage.getStrideY();
  }

  @Override
  public ByteBuffer getDataU() {
    return yuvImage.getDataY();
  }

  @Override
  public int getStrideV() {
    return yuvImage.getStrideY();
  }

  @Override
  public ByteBuffer getDataV() {
    return yuvImage.getDataY();
  }

  @Override
  public int getStrideA() {
    return strideA;
  }

  @Override
  public ByteBuffer getDataA() {
    return dataA.slice();
  }

  @Override
  public I420Buffer toI420() {
    return yuvImage;
  }

  @Override
  public void retain() {
    synchronized (refCountLock) {
      ++refCount;
    }
  }

  @Override
  public void release() {
    synchronized (refCountLock) {
      if (--refCount == 0 && releaseCallback != null) {
        releaseCallback.run();
      }
    }
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    JavaI420Buffer newYuvImage = (JavaI420Buffer) VideoFrame.cropAndScaleI420(
        yuvImage, cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);

    ByteBuffer newAlphaBuffer;
    // For no scale case, no copy necessary but we need to increase ref count.
    if (cropWidth == scaleWidth && cropHeight == scaleHeight) {
      newAlphaBuffer = dataA.slice();
      newAlphaBuffer.position(cropX + cropY * strideA);
      newAlphaBuffer = newAlphaBuffer.slice();

      retain();
      return JavaI420ABuffer.wrap(scaleWidth, scaleHeight, newYuvImage.getDataY(),
          newYuvImage.getStrideY(), newYuvImage.getDataU(), newYuvImage.getStrideU(),
          newYuvImage.getDataV(), newYuvImage.getStrideV(), newAlphaBuffer, strideA,
          this ::release);
    }

    newAlphaBuffer = ByteBuffer.allocateDirect(scaleWidth * scaleHeight);

    VideoFrame.nativeCropAndScalePlane(dataA, strideA, cropX, cropY, cropWidth, cropHeight,
        newAlphaBuffer, scaleWidth, scaleWidth, scaleHeight);

    return JavaI420ABuffer.wrap(scaleWidth, scaleHeight, newYuvImage.getDataY(),
        newYuvImage.getStrideY(), newYuvImage.getDataU(), newYuvImage.getStrideU(),
        newYuvImage.getDataV(), newYuvImage.getStrideV(), newAlphaBuffer, strideA, null);
  }
}