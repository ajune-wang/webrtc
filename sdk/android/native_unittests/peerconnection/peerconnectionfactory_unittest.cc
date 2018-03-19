/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/native_api/peerconnection/peerconnectionfactory.h"

#include "media/base/mediaengine.h"
#include "sdk/android/src/jni/androidnetworkmonitor.h"
#include "sdk/android/src/jni/pc/audio.h"
#include "sdk/android/src/jni/pc/media.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

// Create native peer connection factory, that will be wrapped by java one
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreateTestPCF(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread) {
  // talk/ assumes pretty widely that the current Thread is ThreadManager'd, but
  // ThreadManager only WrapCurrentThread()s the thread where it is first
  // created.  Since the semantics around when auto-wrapping happens in
  // webrtc/rtc_base/ are convoluted, we simply wrap here to avoid having to
  // think about ramifications of auto-wrapping there.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::NetworkMonitorFactory* network_monitor_factory = nullptr;
  auto audio_encoder_factory = jni::CreateAudioEncoderFactory();
  auto audio_decoder_factory = jni::CreateAudioDecoderFactory();
  auto audio_processor = jni::CreateAudioProcessing();

  network_monitor_factory = new jni::AndroidNetworkMonitorFactory();
  rtc::NetworkMonitorFactory::SetFactory(network_monitor_factory);
  std::unique_ptr<CallFactoryInterface> call_factory(CreateCallFactory());
  std::unique_ptr<RtcEventLogFactoryInterface> rtc_event_log_factory(
      CreateRtcEventLogFactory());

  AudioDeviceModule* adm = nullptr;
  rtc::scoped_refptr<AudioMixer> audio_mixer = nullptr;

  cricket::WebRtcVideoEncoderFactory* legacy_video_encoder_factory = nullptr;
  cricket::WebRtcVideoDecoderFactory* legacy_video_decoder_factory = nullptr;
  std::unique_ptr<cricket::MediaEngineInterface> media_engine;
  media_engine.reset(jni::CreateMediaEngine(
      adm, audio_encoder_factory, audio_decoder_factory,
      legacy_video_encoder_factory, legacy_video_decoder_factory, audio_mixer,
      audio_processor));

  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      CreateModularPeerConnectionFactory(
          network_thread, worker_thread, signaling_thread,
          std::move(media_engine), std::move(call_factory),
          std::move(rtc_event_log_factory)));
  RTC_CHECK(factory) << "Failed to create the peer connection factory; "
                     << "WebRTC/libjingle init likely failed on this device";

  return factory;
}

TEST(PeerConnectionFactoryTest, NativeToJavaPeerConnectionFactory) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  // Create threads.
  std::unique_ptr<rtc::Thread> network_thread =
      rtc::Thread::CreateWithSocketServer();
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread->Start()) << "Failed to start thread";

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      CreateTestPCF(network_thread.get(), worker_thread.get(),
                    signaling_thread.get());

  jobject java_factory = NativeToJavaPeerConnectionFactory(
      env, factory, std::move(network_thread), std::move(worker_thread),
      std::move(signaling_thread), nullptr, nullptr, nullptr);

  EXPECT_TRUE(java_factory != nullptr);
}

}  // namespace
}  // namespace test
}  // namespace webrtc
