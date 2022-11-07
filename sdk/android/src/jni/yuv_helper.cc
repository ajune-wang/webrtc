/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "sdk/android/generated_video_jni/YuvHelper_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace webrtc {
namespace jni {

void JNI_YuvHelper_CopyPlane(JNIEnv* jni,
                             const JavaParamRef<jobject>& j_src,
                             jint src_stride,
                             const JavaParamRef<jobject>& j_dst,
                             jint dst_stride,
                             jint width,
                             jint height) {
  const uint8_t* src =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src.obj()));
  uint8_t* dst =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst.obj()));

  libyuv::CopyPlane(src, src_stride, dst, dst_stride, width, height);
}

void JNI_YuvHelper_I420Copy(JNIEnv* jni,
                            const JavaParamRef<jobject>& j_src_y,
                            jint src_stride_y,
                            const JavaParamRef<jobject>& j_src_u,
                            jint src_stride_u,
                            const JavaParamRef<jobject>& j_src_v,
                            jint src_stride_v,
                            const JavaParamRef<jobject>& j_dst_y,
                            jint dst_stride_y,
                            const JavaParamRef<jobject>& j_dst_u,
                            jint dst_stride_u,
                            const JavaParamRef<jobject>& j_dst_v,
                            jint dst_stride_v,
                            jint width,
                            jint height) {
  const uint8_t* src_y =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_y.obj()));
  const uint8_t* src_u =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_u.obj()));
  const uint8_t* src_v =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_v.obj()));
  uint8_t* dst_y =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y.obj()));
  uint8_t* dst_u =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_u.obj()));
  uint8_t* dst_v =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_v.obj()));

  libyuv::I420Copy(src_y, src_stride_y, src_u, src_stride_u, src_v,
                   src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                   dst_v, dst_stride_v, width, height);
}

static void JNI_YuvHelper_I420ToNV12(JNIEnv* jni,
                                     const JavaParamRef<jobject>& j_src_y,
                                     jint src_stride_y,
                                     const JavaParamRef<jobject>& j_src_u,
                                     jint src_stride_u,
                                     const JavaParamRef<jobject>& j_src_v,
                                     jint src_stride_v,
                                     const JavaParamRef<jobject>& j_dst_y,
                                     jint dst_stride_y,
                                     const JavaParamRef<jobject>& j_dst_uv,
                                     jint dst_stride_uv,
                                     jint width,
                                     jint height) {
  const uint8_t* src_y =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_y.obj()));
  const uint8_t* src_u =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_u.obj()));
  const uint8_t* src_v =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_v.obj()));
  uint8_t* dst_y =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y.obj()));
  uint8_t* dst_uv =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_uv.obj()));

  libyuv::I420ToNV12(src_y, src_stride_y, src_u, src_stride_u, src_v,
                     src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                     width, height);
}

void JNI_YuvHelper_I420Rotate(JNIEnv* jni,
                              const JavaParamRef<jobject>& j_src_y,
                              jint src_stride_y,
                              const JavaParamRef<jobject>& j_src_u,
                              jint src_stride_u,
                              const JavaParamRef<jobject>& j_src_v,
                              jint src_stride_v,
                              const JavaParamRef<jobject>& j_dst_y,
                              jint dst_stride_y,
                              const JavaParamRef<jobject>& j_dst_u,
                              jint dst_stride_u,
                              const JavaParamRef<jobject>& j_dst_v,
                              jint dst_stride_v,
                              jint src_width,
                              jint src_height,
                              jint rotation_mode) {
  const uint8_t* src_y =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_y.obj()));
  const uint8_t* src_u =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_u.obj()));
  const uint8_t* src_v =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_v.obj()));
  uint8_t* dst_y =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y.obj()));
  uint8_t* dst_u =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_u.obj()));
  uint8_t* dst_v =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_v.obj()));

  libyuv::I420Rotate(src_y, src_stride_y, src_u, src_stride_u, src_v,
                     src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                     dst_v, dst_stride_v, src_width, src_height,
                     static_cast<libyuv::RotationMode>(rotation_mode));
}

static void JNI_YuvHelper_I420Scale(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& srcYBuffer,
    jint srcYStride,
    const base::android::JavaParamRef<jobject>& srcUBuffer,
    jint srcUStride,
    const base::android::JavaParamRef<jobject>& srcVBuffer,
    jint srcVStride,
    jint srcWidth,
    jint srcHeight,
    const base::android::JavaParamRef<jobject>& dstYBuffer,
    jint dstYStride,
    const base::android::JavaParamRef<jobject>& dstUBuffer,
    jint dstUStride,
    const base::android::JavaParamRef<jobject>& dstVBuffer,
    jint dstVStride,
    jint dstWidth,
    jint dstHeight) {
  const uint8_t* srcYData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(srcYBuffer.obj()));
  const uint8_t* srcUData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(srcUBuffer.obj()));
  const uint8_t* srcVData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(srcVBuffer.obj()));
  uint8_t* dstYData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(dstYBuffer.obj()));
  uint8_t* dstUData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(dstUBuffer.obj()));
  uint8_t* dstVData =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(dstVBuffer.obj()));
  libyuv::I420Scale(srcYData, srcYStride, srcUData, srcUStride, srcVData,
                    srcVStride, srcWidth, srcHeight, dstYData, dstYStride,
                    dstUData, dstUStride, dstVData, dstVStride, dstWidth,
                    dstHeight, libyuv::kFilterBilinear);
}

void JNI_YuvHelper_ABGRToI420(JNIEnv* jni,
                              const JavaParamRef<jobject>& j_src,
                              jint src_stride,
                              const JavaParamRef<jobject>& j_dst_y,
                              jint dst_stride_y,
                              const JavaParamRef<jobject>& j_dst_u,
                              jint dst_stride_u,
                              const JavaParamRef<jobject>& j_dst_v,
                              jint dst_stride_v,
                              jint src_width,
                              jint src_height) {
  const uint8_t* src =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src.obj()));
  uint8_t* dst_y =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y.obj()));
  uint8_t* dst_u =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_u.obj()));
  uint8_t* dst_v =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_v.obj()));

  libyuv::ABGRToI420(src, src_stride, dst_y, dst_stride_y, dst_u, dst_stride_u,
                     dst_v, dst_stride_v, src_width, src_height);
}

}  // namespace jni
}  // namespace webrtc
