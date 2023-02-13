/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/i420_buffer.h"
#include "sdk/android/generated_test_native_utils_jni/VideoFrameBufferTestNativeUtils_jni.h"
#include "sdk/android/src/jni/video_frame.h"
#include "sdk/android/src/jni/wrapped_native_i420_buffer.h"

namespace webrtc {
namespace jni {

static jint JNI_VideoFrameBufferTestNativeUtils_GetBufferType(
    JNIEnv* env,
    const const JavaParamRef<jobject>& video_frame_buffer) {
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      JavaToNativeFrameBuffer(env, video_frame_buffer);
  return static_cast<jint>(buffer->type());
}

static ScopedJavaLocalRef<jobject>
JNI_VideoFrameBufferTestNativeUtils_GetNativeI420Buffer(
    JNIEnv* env,
    const JavaParamRef<jobject>& i420_buffer) {
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      JavaToNativeFrameBuffer(env, i420_buffer);
  const I420BufferInterface* inputBuffer = buffer->GetI420();
  RTC_DCHECK(inputBuffer != nullptr);
  rtc::scoped_refptr<I420Buffer> outputBuffer = I420Buffer::Copy(*inputBuffer);
  return WrapI420Buffer(env, outputBuffer);
}

}  // namespace jni
}  // namespace webrtc
