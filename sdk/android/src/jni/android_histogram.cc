/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>

#include "sdk/android/generated_base_jni/jni/Histogram_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "system_wrappers/include/metrics.h"

// Enables collection of native histograms and creating them.
namespace webrtc {
namespace jni {

static jlong JNI_Histogram_CreateCounts(JNIEnv* jni,
                                        const JavaParamRef<jstring>& j_name,
                                        jint min,
                                        jint max,
                                        jint buckets) {
  std::string name = JavaToStdString(jni, j_name);
  return jlongFromPointer(
      metrics_internal::HistogramFactoryGetCounts(name, min, max, buckets));
}

static jlong JNI_Histogram_CreateEnumeration(
    JNIEnv* jni,
    const JavaParamRef<jstring>& j_name,
    jint max) {
  std::string name = JavaToStdString(jni, j_name);
  return jlongFromPointer(
      metrics_internal::HistogramFactoryGetEnumeration(name, max));
}

static void JNI_Histogram_AddSample(JNIEnv* jni,
                                    jlong histogram,
                                    jint sample) {
  if (histogram) {
    HistogramAdd(reinterpret_cast<metrics_internal::Histogram*>(histogram),
                 sample);
  }
}

}  // namespace jni
}  // namespace webrtc
