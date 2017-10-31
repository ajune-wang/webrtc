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

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/videosourceproxy.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/androidmediadecoder_jni.h"
#include "sdk/android/src/jni/androidmediaencoder_jni.h"
#include "sdk/android/src/jni/androidvideotracksource.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/pc/ownedfactoryandthreads.h"
#include "sdk/android/src/jni/surfacetexturehelper_jni.h"
#include "sdk/android/src/jni/videodecoderfactorywrapper.h"
#include "sdk/android/src/jni/videoencoderfactorywrapper.h"

namespace webrtc {
namespace jni {

std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(
    JNIEnv* jni,
    jobject j_encoder_factory) {
  return std::unique_ptr<VideoEncoderFactory>(
      new VideoEncoderFactoryWrapper(jni, j_encoder_factory));
}

std::unique_ptr<VideoDecoderFactory> CreateVideoDecoderFactory(
    JNIEnv* jni,
    jobject j_decoder_factory) {
  return std::unique_ptr<VideoDecoderFactory>(
      new VideoDecoderFactoryWrapper(jni, j_decoder_factory));
}

cricket::WebRtcVideoEncoderFactory* CreateLegacyVideoEncoderFactory() {
  return new MediaCodecVideoEncoderFactory();
}

cricket::WebRtcVideoDecoderFactory* CreateLegacyVideoDecoderFactory() {
  return new MediaCodecVideoDecoderFactory();
}

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper) {
  return surface_texture_helper
             ? surface_texture_helper->GetJavaSurfaceTextureHelper()
             : nullptr;
}

JNI_FUNCTION_DECLARATION(jlong,
                         PeerConnectionFactory_nativeCreateVideoSource,
                         JNIEnv* jni,
                         jclass,
                         jlong native_factory,
                         jobject j_surface_texture_helper,
                         jboolean is_screencast) {
  OwnedFactoryAndThreads* factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);

  rtc::scoped_refptr<AndroidVideoTrackSource> source(
      new rtc::RefCountedObject<AndroidVideoTrackSource>(
          factory->signaling_thread(), jni, j_surface_texture_helper,
          is_screencast));
  rtc::scoped_refptr<VideoTrackSourceProxy> proxy_source =
      VideoTrackSourceProxy::Create(factory->signaling_thread(),
                                    factory->worker_thread(), source);

  return (jlong)proxy_source.release();
}

JNI_FUNCTION_DECLARATION(jlong,
                         PeerConnectionFactory_nativeCreateVideoTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong native_factory,
                         jstring id,
                         jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<VideoTrackInterface> track(factory->CreateVideoTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<VideoTrackSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JNI_FUNCTION_DECLARATION(
    void,
    PeerConnectionFactory_nativeSetVideoHwAccelerationOptions,
    JNIEnv* jni,
    jclass,
    jlong native_factory,
    jobject local_egl_context,
    jobject remote_egl_context) {
  OwnedFactoryAndThreads* owned_factory =
      reinterpret_cast<OwnedFactoryAndThreads*>(native_factory);

  jclass j_eglbase14_context_class =
      FindClass(jni, "org/webrtc/EglBase14$Context");

  if (owned_factory->legacy_encoder_factory()) {
    MediaCodecVideoEncoderFactory* encoder_factory =
        static_cast<MediaCodecVideoEncoderFactory*>(
            owned_factory->legacy_encoder_factory());
    if (encoder_factory &&
        jni->IsInstanceOf(local_egl_context, j_eglbase14_context_class)) {
      LOG(LS_INFO) << "Set EGL context for HW encoding.";
      encoder_factory->SetEGLContext(jni, local_egl_context);
    }
  }

  if (owned_factory->legacy_decoder_factory()) {
    MediaCodecVideoDecoderFactory* decoder_factory =
        static_cast<MediaCodecVideoDecoderFactory*>(
            owned_factory->legacy_decoder_factory());
    if (decoder_factory) {
      LOG(LS_INFO) << "Set EGL context for HW decoding.";
      decoder_factory->SetEGLContext(jni, remote_egl_context);
    }
  }
}

}  // namespace jni
}  // namespace webrtc
