/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
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
public class JavaI420ABuffer extends JavaI420Buffer implements VideoFrame.I420ABuffer {
  private final ByteBuffer dataA;
  private final int strideA;

  private JavaI420ABuffer(int width, int height, ByteBuffer dataY, int strideY, ByteBuffer dataU,
      int strideU, ByteBuffer dataV, int strideV, ByteBuffer dataA, int strideA,
      Runnable releaseCallback) {
    super(width, height, dataY, strideY, dataU, strideU, dataV, strideV, releaseCallback);
    this.dataA = dataA;
    this.strideA = strideA;
  }

  /** Wraps existing ByteBuffers into JavaI420Buffer object without copying the contents. */
  public static JavaI420Buffer wrap(int width, int height, ByteBuffer dataY, int strideY,
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
    dataY = dataY.slice();
    dataU = dataU.slice();
    dataV = dataV.slice();
    dataA = dataA.slice();

    final int chromaHeight = (height + 1) / 2;
    final int minCapacityY = strideY * height;
    final int minCapacityU = strideU * chromaHeight;
    final int minCapacityV = strideV * chromaHeight;
    final int minCapacityA = strideA * height;
    if (dataY.capacity() < minCapacityY) {
      throw new IllegalArgumentException("Y-buffer must be at least " + minCapacityY + " bytes.");
    }
    if (dataU.capacity() < minCapacityU) {
      throw new IllegalArgumentException("U-buffer must be at least " + minCapacityU + " bytes.");
    }
    if (dataV.capacity() < minCapacityV) {
      throw new IllegalArgumentException("V-buffer must be at least " + minCapacityV + " bytes.");
    }
    if (dataA.capacity() < minCapacityA) {
      throw new IllegalArgumentException("A-buffer must be at least " + minCapacityA + " bytes.");
    }

    return new JavaI420ABuffer(width, height, dataY, strideY, dataU, strideU, dataV, strideV, dataA,
        strideA, releaseCallback);
  }

  /** Allocates an empty I420Buffer suitable for an image of the given dimensions. */
  public static JavaI420Buffer allocate(int width, int height) {
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
  public int getStrideA() {
    return strideA;
  }

  @Override
  public ByteBuffer getDataA() {
    return dataA.slice();
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    android.util.Log.e("JavaI420ABuffer", "Warning: cropAndScale losts alpha data.");
    return VideoFrame.cropAndScaleI420(
        this, cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
  }
}
