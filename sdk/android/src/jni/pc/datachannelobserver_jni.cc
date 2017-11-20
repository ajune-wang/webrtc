/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/datachannelobserver_jni.h"

#include "sdk/android/generated_peerconnection_jni/jni/DataChannel_jni.h"

namespace webrtc {
namespace jni {

DataChannelObserverJni::DataChannelObserverJni(JNIEnv* jni, jobject j_observer)
    : j_observer_global_(jni, j_observer) {}

void DataChannelObserverJni::OnBufferedAmountChange(uint64_t previous_amount) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_DataChannel_Observer_onBufferedAmountChange(env, *j_observer_global_,
                                                   previous_amount);
}

void DataChannelObserverJni::OnStateChange() {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_DataChannel_Observer_onStateChange(env, *j_observer_global_);
}

void DataChannelObserverJni::OnMessage(const DataBuffer& buffer) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject byte_buffer = env->NewDirectByteBuffer(
      const_cast<char*>(buffer.data.data<char>()), buffer.data.size());
  jobject j_buffer =
      Java_DataChannel_Buffer_create(env, byte_buffer, buffer.binary);
  Java_DataChannel_Observer_onMessage(env, *j_observer_global_, j_buffer);
}

}  // namespace jni
}  // namespace webrtc
