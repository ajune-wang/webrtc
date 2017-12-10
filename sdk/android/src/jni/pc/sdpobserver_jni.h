/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "sdk/android/generated_peerconnection_jni/jni/SdpObserver_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Adapter for a Java StatsObserver presenting a C++
// CreateSessionDescriptionObserver or SetSessionDescriptionObserver and
// dispatching the callback from C++ back to Java.
template <class T>  // T is one of {Create,Set}SessionDescriptionObserver.
class SdpObserverJni : public T {
 public:
  SdpObserverJni(JNIEnv* env,
                 jobject j_observer,
                 std::unique_ptr<MediaConstraintsInterface> constraints)
      : j_observer_global_(env, j_observer),
        constraints_(std::move(constraints)) {}

  virtual ~SdpObserverJni() {}

  // Can't mark override because of templating.
  virtual void OnSuccess() {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    Java_SdpObserver_onSetSuccess(env, *j_observer_global_);
  }

  // Can't mark override because of templating.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    ScopedLocalRefFrame local_ref_frame(env);
    Java_SdpObserver_onCreateSuccess(env, *j_observer_global_,
                                     NativeToJavaSessionDescription(env, desc));
    // OnSuccess transfers ownership of the description (there's a TODO to make
    // it use unique_ptr...).
    delete desc;
  }

  MediaConstraintsInterface* constraints() { return constraints_.get(); }

 protected:
  const ScopedGlobalRef<jobject> j_observer_global_;

 private:
  std::unique_ptr<MediaConstraintsInterface> constraints_;
};

class CreateSdpObserverJni
    : public SdpObserverJni<CreateSessionDescriptionObserver> {
 public:
  CreateSdpObserverJni(JNIEnv* env,
                       jobject j_observer,
                       std::unique_ptr<MediaConstraintsInterface> constraints)
      : SdpObserverJni(env, j_observer, std::move(constraints)) {}

  void OnFailure(const std::string& error) override {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    Java_SdpObserver_onCreateFailure(env, *j_observer_global_,
                                     NativeToJavaString(env, error));
  }
};

class SetSdpObserverJni : public SdpObserverJni<SetSessionDescriptionObserver> {
 public:
  SetSdpObserverJni(JNIEnv* env,
                    jobject j_observer,
                    std::unique_ptr<MediaConstraintsInterface> constraints)
      : SdpObserverJni(env, j_observer, std::move(constraints)) {}

  void OnFailure(const std::string& error) override {
    JNIEnv* env = AttachCurrentThreadIfNeeded();
    Java_SdpObserver_onSetFailure(env, *j_observer_global_,
                                  NativeToJavaString(env, error));
  }
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_
