/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_MEDIASTREAM_H_
#define SDK_ANDROID_SRC_JNI_PC_MEDIASTREAM_H_

#include "api/mediastreaminterface.h"
#include "sdk/android/src/jni/scoped_java_ref.h"

namespace webrtc {
namespace jni {

class GlobalJavaMediaStream {
 public:
  explicit GlobalJavaMediaStream(
      JNIEnv* env,
      rtc::scoped_refptr<MediaStreamInterface> media_stream);
  ~GlobalJavaMediaStream();

  ScopedJavaGlobalRef<jobject>& j_media_stream() { return j_media_stream_; }

 private:
  ScopedJavaGlobalRef<jobject> j_media_stream_;
};

jclass GetMediaStreamClass(JNIEnv* env);

void AddNativeAudioTrackToJavaStream(
    JNIEnv* env,
    rtc::scoped_refptr<AudioTrackInterface> track,
    const JavaRef<jobject>& j_stream);

void AddNativeVideoTrackToJavaStream(
    JNIEnv* env,
    rtc::scoped_refptr<VideoTrackInterface> track,
    const JavaRef<jobject>& j_stream);

void RemoveAudioTrackFromStream(JNIEnv* env,
                                AudioTrackInterface* track,
                                const JavaRef<jobject>& j_media_stream);

void RemoveVideoTrackFromStream(JNIEnv* env,
                                VideoTrackInterface* track,
                                const JavaRef<jobject>& j_media_stream);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_MEDIASTREAM_H_
