/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/androidnativeapi/jni/androidcallclient.h"

#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peerconnectioninterface.h"
#include "examples/androidnativeapi/generated_jni/jni/CallClient_jni.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/test/fakeperiodicvideocapturer.h"
#include "rtc_base/ptr_util.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/video/wrapper.h"

namespace webrtc_examples {

AndroidCallClient::AndroidCallClient()
    : pc_observer_(this),
      create_offer_observer_(
          new rtc::RefCountedObject<CreateOfferObserver>(this)),
      set_remote_session_description_observer_(
          new rtc::RefCountedObject<SetRemoteSessionDescriptionObserver>()),
      set_local_session_description_observer_(
          new rtc::RefCountedObject<SetLocalSessionDescriptionObserver>()) {
  CreatePeerConnectionFactory();
}

void AndroidCallClient::Call(JNIEnv* env,
                             const webrtc::JavaRef<jobject>& cls,
                             const webrtc::JavaRef<jobject>& local_sink,
                             const webrtc::JavaRef<jobject>& remote_sink) {
  if (pc_ != nullptr) {
    RTC_LOG(LS_WARNING) << "Call already started.";
    return;
  }

  local_sink_ = webrtc::JavaToNativeVideoSink(env, local_sink.obj());
  remote_sink_ = webrtc::JavaToNativeVideoSink(env, remote_sink.obj());

  // The fake video source wants to be created on the same thread as it is
  // destroyed. It is destroyed on the signaling thread so we have to invoke
  // here.
  signaling_thread_->Invoke<void>(RTC_FROM_HERE, [&] {
    // TODO(sakal): Get picture from camera?
    video_source_ = pcf_->CreateVideoSource(
        rtc::MakeUnique<webrtc::FakePeriodicVideoCapturer>());
  });

  CreatePeerConnection();
  Connect();
}

void AndroidCallClient::Hangup(JNIEnv* env,
                               const webrtc::JavaRef<jobject>& cls) {
  if (pc_ != nullptr) {
    pc_->Close();
    pc_ = nullptr;
  }

  local_sink_ = nullptr;
  remote_sink_ = nullptr;
  video_source_ = nullptr;
}

void AndroidCallClient::Delete(JNIEnv* env,
                               const webrtc::JavaRef<jobject>& cls) {
  delete this;
}

void AndroidCallClient::CreatePeerConnectionFactory() {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->SetName("network_thread_", nullptr);
  RTC_CHECK(network_thread_->Start()) << "Failed to start thread";

  worker_thread_ = rtc::Thread::Create();
  worker_thread_->SetName("worker_thread_", nullptr);
  RTC_CHECK(worker_thread_->Start()) << "Failed to start thread";

  signaling_thread_ = rtc::Thread::Create();
  signaling_thread_->SetName("signaling_thread_", NULL);
  RTC_CHECK(signaling_thread_->Start()) << "Failed to start thread";

  std::unique_ptr<cricket::MediaEngineInterface> media_engine =
      cricket::WebRtcMediaEngineFactory::Create(
          nullptr /* adm */, webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateBuiltinAudioDecoderFactory(),
          rtc::MakeUnique<webrtc::InternalEncoderFactory>(),
          rtc::MakeUnique<webrtc::InternalDecoderFactory>(),
          nullptr /* audio_mixer */, webrtc::AudioProcessingBuilder().Create());
  RTC_LOG(LS_INFO) << "Media engine created: " << media_engine.get();

  pcf_ = CreateModularPeerConnectionFactory(
      network_thread_.get(), worker_thread_.get(), signaling_thread_.get(),
      std::move(media_engine), webrtc::CreateCallFactory(),
      webrtc::CreateRtcEventLogFactory());
  RTC_LOG(LS_INFO) << "PeerConnectionFactory created: " << pcf_;
}

void AndroidCallClient::CreatePeerConnection() {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // DTLS SRTP has to be disabled for loopback to work.
  config.enable_dtls_srtp = false;
  pc_ = pcf_->CreatePeerConnection(config, nullptr /* port_allocator */,
                                   nullptr /* cert_generator */, &pc_observer_);
  RTC_LOG(LS_INFO) << "PeerConnection created: " << pc_;

  rtc::scoped_refptr<webrtc::VideoTrackInterface> local_video_track =
      pcf_->CreateVideoTrack("video", video_source_);
  local_video_track->AddOrUpdateSink(local_sink_.get(), rtc::VideoSinkWants());
  pc_->AddTransceiver(local_video_track);
  RTC_LOG(LS_INFO) << "Local video sink set up: " << local_video_track;

  for (const rtc::scoped_refptr<webrtc::RtpTransceiverInterface>& tranceiver :
       pc_->GetTransceivers()) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track =
        tranceiver->receiver()->track();
    if (track &&
        track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      static_cast<webrtc::VideoTrackInterface*>(track.get())
          ->AddOrUpdateSink(remote_sink_.get(), rtc::VideoSinkWants());
      RTC_LOG(LS_INFO) << "Remote video sink set up: " << track;
      break;
    }
  }
}

void AndroidCallClient::Connect() {
  pc_->CreateOffer(create_offer_observer_.get(),
                   webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

AndroidCallClient::PCObserver::PCObserver(AndroidCallClient* client)
    : client_(client) {}

void AndroidCallClient::PCObserver::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "OnSignalingChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  RTC_LOG(LS_INFO) << "OnDataChannel";
}

void AndroidCallClient::PCObserver::OnRenegotiationNeeded() {
  RTC_LOG(LS_INFO) << "OnRenegotiationNeeded";
}

void AndroidCallClient::PCObserver::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "OnIceConnectionChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "OnIceGatheringChange: " << new_state;
}

void AndroidCallClient::PCObserver::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << "OnIceCandidate: " << candidate->server_url();
  client_->pc_->AddIceCandidate(candidate);
}

AndroidCallClient::CreateOfferObserver::CreateOfferObserver(
    AndroidCallClient* client)
    : client_(client) {}

void AndroidCallClient::CreateOfferObserver::OnSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  std::string sdp;
  desc->ToString(&sdp);
  RTC_LOG(LS_INFO) << "Created offer: " << sdp;

  // Ownership of desc was transferred to us, now we transfer it forward.
  client_->pc_->SetLocalDescription(
      client_->set_local_session_description_observer_, desc);

  // Generate a fake answer.
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer(
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp));
  client_->pc_->SetRemoteDescription(
      std::move(answer), client_->set_remote_session_description_observer_);
}

void AndroidCallClient::CreateOfferObserver::OnFailure(
    const std::string& error) {
  RTC_LOG(LS_INFO) << "Failed to create offer: " << error;
}

void AndroidCallClient::SetRemoteSessionDescriptionObserver::
    OnSetRemoteDescriptionComplete(webrtc::RTCError error) {
  RTC_LOG(LS_INFO) << "Set remote description: " << error.message();
}

void AndroidCallClient::SetLocalSessionDescriptionObserver::OnSuccess() {
  RTC_LOG(LS_INFO) << "Set local description success!";
}

void AndroidCallClient::SetLocalSessionDescriptionObserver::OnFailure(
    const std::string& error) {
  RTC_LOG(LS_INFO) << "Set local description failure: " << error;
}

static jlong JNI_CallClient_CreateClient(
    JNIEnv* env,
    const webrtc::JavaParamRef<jclass>& cls) {
  return webrtc::NativeToJavaPointer(new webrtc_examples::AndroidCallClient());
}

}  // namespace webrtc_examples
