/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.IntDef;
import androidx.test.filters.SmallTest;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class YuvConverterTest {
  private static final String TAG = "YuvConverterTest";
  private static final int MAX_ALIGNMENT = 16;
  private static final int WIDTH = 48;
  private static final int HEIGHT = 32;

  /**
   * These tests are parameterized on this enum which represents the different VideoFrame.Buffers.
   */
  private static enum BufferType { RGB_TEXTURE, OES_TEXTURE }

  @ClassParameter private static List<ParameterSet> CLASS_PARAMS = new ArrayList<>();

  static {
    final int[] alignments = new int[] {1, 2, 4, 8, 16};
    for (BufferType bufferType : BufferType.values()) {
      for (int widthAlignment : alignments) {
        for (int heightAlignment : alignments) {
          String name = String.format(
              Locale.US, "%s_%d_%d", bufferType.name(), widthAlignment, heightAlignment);
          CLASS_PARAMS.add(new ParameterSet()
                               .value(bufferType.name(), widthAlignment, heightAlignment)
                               .name(name));
        }
      }
    }
  }

  @BeforeClass
  public static void setUp() {
    // Needed for JniCommon.nativeAllocateByteBuffer() to work, which is used from JavaI420Buffer.
    System.loadLibrary(TestConstants.NATIVE_LIBRARY);
  }

  private final BufferType bufferType;
  private final int width;
  private final int height;

  public YuvConverterTest(String bufferTypeName, int widthAlignment, int heightAlignment) {
    // Parse the string back to an enum.
    bufferType = BufferType.valueOf(bufferTypeName);
    this.width = WIDTH - MAX_ALIGNMENT + widthAlignment;
    this.height = HEIGHT - MAX_ALIGNMENT + heightAlignment;
    if (this.width % MAX_ALIGNMENT != (widthAlignment % MAX_ALIGNMENT)) {
      throw new IllegalStateException("width does not satisfy alignment");
    }
    if (this.height % MAX_ALIGNMENT != (heightAlignment % MAX_ALIGNMENT)) {
      throw new IllegalStateException("height does not satisfy alignment");
    }
  }

  private static VideoFrame.Buffer createBufferWithType(
      BufferType bufferType, VideoFrame.I420Buffer i420Buffer) {
    switch (bufferType) {
      case RGB_TEXTURE:
        return VideoFrameBufferTest.createRgbTextureBuffer(/* eglContext= */ null, i420Buffer);
      case OES_TEXTURE:
        return VideoFrameBufferTest.createOesTextureBuffer(/* eglContext= */ null, i420Buffer);
      default:
        throw new IllegalArgumentException("Unknown buffer type: " + bufferType);
    }
  }

  private VideoFrame.Buffer createBufferToTest(VideoFrame.I420Buffer i420Buffer) {
    return createBufferWithType(this.bufferType, i420Buffer);
  }

  public static VideoFrame.I420Buffer createTestI420Buffer(int width, int height) {
    // VideoFrameBufferTest always create Buffer with 16x16 pixels
    final int smallBufferSize = 16;
    final VideoFrame.I420Buffer smallBuffer = VideoFrameBufferTest.createTestI420Buffer();

    final VideoFrame.Buffer scaledBuffer =
        smallBuffer.cropAndScale(0, 0, smallBufferSize, smallBufferSize, width, height);
    smallBuffer.release();

    final VideoFrame.I420Buffer resultBuffer = scaledBuffer.toI420();
    scaledBuffer.release();

    return resultBuffer;
  }

  @Test
  @SmallTest
  public void testValidateI420Conversion() {
    final VideoFrame.I420Buffer referenceI420Buffer = createTestI420Buffer(width, height);
    final VideoFrame.Buffer bufferToTest = createBufferToTest(referenceI420Buffer);

    final VideoFrame.I420Buffer outputI420Buffer = bufferToTest.toI420();
    bufferToTest.release();

    VideoFrameBufferTest.assertAlmostEqualI420Buffers(referenceI420Buffer, outputI420Buffer);
    referenceI420Buffer.release();
    outputI420Buffer.release();
  }
}
