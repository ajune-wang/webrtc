/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/mediastream.h"

#include "sdk/android/generated_peerconnection_jni/jni/MediaStream_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Java MediaStream holds one reference. Corresponding Release() is in
// MediaStream_free, triggered by MediaStream.dispose().
GlobalJavaMediaStream::GlobalJavaMediaStream(
    JNIEnv* env,
    rtc::scoped_refptr<MediaStreamInterface> media_stream)
    : j_media_stream_(env,
                      Java_MediaStream_Constructor(
                          env,
                          jlongFromPointer(media_stream.release()))) {}

GlobalJavaMediaStream::~GlobalJavaMediaStream() {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_dispose(env, *j_media_stream_);
}

jclass GetMediaStreamClass(JNIEnv* env) {
  return org_webrtc_MediaStream_clazz(env);
}

void AddNativeAudioTrackToJavaStream(
    JNIEnv* env,
    rtc::scoped_refptr<AudioTrackInterface> track,
    jobject j_stream) {
  Java_MediaStream_addNativeAudioTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void AddNativeVideoTrackToJavaStream(
    JNIEnv* env,
    rtc::scoped_refptr<VideoTrackInterface> track,
    jobject j_stream) {
  Java_MediaStream_addNativeVideoTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void RemoveAudioTrackFromStream(JNIEnv* env,
                                AudioTrackInterface* track,
                                jobject j_media_stream) {
  Java_MediaStream_removeAudioTrack(env, j_media_stream,
                                    jlongFromPointer(track));
}

void RemoveVideoTrackFromStream(JNIEnv* env,
                                VideoTrackInterface* track,
                                jobject j_media_stream) {
  Java_MediaStream_removeVideoTrack(env, j_media_stream,
                                    jlongFromPointer(track));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_addAudioTrackToNativeStream,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_addVideoTrackToNativeStream,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_removeNativeAudioTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_removeNativeVideoTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStream_getNativeLabel,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return NativeToJavaString(
      jni, reinterpret_cast<MediaStreamInterface*>(j_p)->label());
}

JNI_FUNCTION_DECLARATION(void, MediaStream_free, JNIEnv*, jclass, jlong j_p) {
  reinterpret_cast<MediaStreamInterface*>(j_p)->Release();
}

}  // namespace jni
}  // namespace webrtc
