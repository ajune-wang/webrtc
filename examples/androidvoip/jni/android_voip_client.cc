/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/androidvoip/jni/android_voip_client.h"

#include <map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/voip/voip_engine_factory.h"
#include "examples/androidvoip/generated_jni/VoipClient_jni.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/java_types.h"

namespace webrtc_examples {

AndroidVoipClient::AndroidVoipClient() = default;

AndroidVoipClient::~AndroidVoipClient() {
  rtp_socket_thread_->Stop();
  rtcp_socket_thread_->Stop();
}

bool AndroidVoipClient::Initialize(JNIEnv* env) {
  rtp_socket_thread_ = rtc::Thread::CreateWithSocketServer();
  rtcp_socket_thread_ = rtc::Thread::CreateWithSocketServer();
  rtp_socket_thread_->Start();
  rtcp_socket_thread_->Start();

  bool res = true;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> builtInEncoderFactory =
        webrtc::CreateBuiltinAudioEncoderFactory();
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> builtInDecoderFactory =
        webrtc::CreateBuiltinAudioDecoderFactory();

    webrtc::VoipEngineConfig config;
    config.encoder_factory = builtInEncoderFactory;
    config.decoder_factory = builtInDecoderFactory;
    config.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();

    config.audio_device_module = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kPlatformDefaultAudio,
        config.task_queue_factory.get());
    if (!config.audio_device_module) {
      res = false;
      return;
    }

    config.audio_processing = webrtc::AudioProcessingBuilder().Create();
    if (!config.audio_processing) {
      res = false;
      return;
    }

    this->supported_encoders_ = builtInEncoderFactory->GetSupportedEncoders();
    this->supported_decoders_ = builtInDecoderFactory->GetSupportedDecoders();

    this->voip_engine_ = webrtc::CreateVoipEngine(std::move(config));
    if (!this->voip_engine_) {
      res = false;
      return;
    }
  });
  return res;
}

webrtc::ScopedJavaLocalRef<jobject> AndroidVoipClient::GetSupportedEncoders(
    JNIEnv* env) {
  return GetCodecNames(env, supported_encoders_);
}

webrtc::ScopedJavaLocalRef<jobject> AndroidVoipClient::GetSupportedDecoders(
    JNIEnv* env) {
  return GetCodecNames(env, supported_decoders_);
}

webrtc::ScopedJavaLocalRef<jstring> AndroidVoipClient::GetLocalIPAddress(
    JNIEnv* env) {
  // prefer ipv4 address over ipv6 address
  rtc::IPAddress ipv4_address = QueryDefaultLocalAddress(AF_INET);
  if (!ipv4_address.IsNil()) {
    return webrtc::NativeToJavaString(env, ipv4_address.ToString());
  }
  rtc::IPAddress ipv6_address = QueryDefaultLocalAddress(AF_INET6);
  if (!ipv6_address.IsNil()) {
    return webrtc::NativeToJavaString(env, ipv6_address.ToString());
  }
  // return empty string if both ipv4 and ipv6 addresses are not found
  return webrtc::NativeToJavaString(env, "");
}

void AndroidVoipClient::SetEncoder(
    JNIEnv* env,
    const webrtc::JavaRef<jstring>& j_encoder_string) {
  std::string chosen_encoder =
      webrtc::JavaToNativeString(env, j_encoder_string);
  for (const webrtc::AudioCodecSpec& encoder : supported_encoders_) {
    if (encoder.format.name == chosen_encoder) {
      voip_engine_->Codec().SetSendCodec(channel_, 0, encoder.format);
      break;
    }
  }
}

void AndroidVoipClient::SetDecoders(
    JNIEnv* env,
    const webrtc::JavaParamRef<jobject>& j_decoder_strings) {
  std::vector<std::string> chosen_decoders =
      webrtc::JavaListToNativeVector<std::string, jstring>(
          env, j_decoder_strings, &webrtc::JavaToNativeString);
  std::unordered_set<std::string> decoder_strings_set(chosen_decoders.begin(),
                                                      chosen_decoders.end());
  std::map<int, webrtc::SdpAudioFormat> decoder_specs;
  int counter = 0;

  for (const webrtc::AudioCodecSpec& decoder : supported_decoders_) {
    if (decoder_strings_set.count(decoder.format.name)) {
      decoder_specs.insert({counter++, decoder.format});
    }
  }

  voip_engine_->Codec().SetReceiveCodecs(channel_, decoder_specs);
}

void AndroidVoipClient::SetLocalAddress(
    JNIEnv* env,
    const webrtc::JavaRef<jstring>& j_ip_address_string,
    jint j_port_number_int) {
  std::string ip_address = webrtc::JavaToNativeString(env, j_ip_address_string);
  rtp_local_address_ = rtc::SocketAddress(ip_address, j_port_number_int);
  rtcp_local_address_ = rtc::SocketAddress(ip_address, j_port_number_int + 1);
}

void AndroidVoipClient::SetRemoteAddress(
    JNIEnv* env,
    const webrtc::JavaRef<jstring>& j_ip_address_string,
    jint j_port_number_int) {
  std::string ip_address = webrtc::JavaToNativeString(env, j_ip_address_string);
  remote_address_ = rtc::SocketAddress(ip_address, j_port_number_int);
}

jboolean AndroidVoipClient::StartSession(JNIEnv* env) {
  absl::optional<webrtc::ChannelId> channel;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &channel] {
    channel = this->voip_engine_->Base().CreateChannel(this, 0);
  });
  if (!channel) {
    return false;
  }
  channel_ = *channel;

  if (!rtp_socket_) {
    rtc::AsyncUDPSocket* udp_socket = rtc::AsyncUDPSocket::Create(
        rtp_socket_thread_->socketserver(), rtp_local_address_);
    if (!udp_socket) {
      return false;
    }
    rtp_socket_.reset(udp_socket);
    rtp_socket_->SignalReadPacket.connect(
        this, &AndroidVoipClient::OnSignalReadRTPPacket);
  }

  if (!rtcp_socket_) {
    rtc::AsyncUDPSocket* udp_socket = rtc::AsyncUDPSocket::Create(
        rtcp_socket_thread_->socketserver(), rtcp_local_address_);
    if (!udp_socket) {
      return false;
    }
    rtcp_socket_.reset(udp_socket);
    rtcp_socket_->SignalReadPacket.connect(
        this, &AndroidVoipClient::OnSignalReadRTCPPacket);
  }
  return true;
}

jboolean AndroidVoipClient::StopSession(JNIEnv* env) {
  bool res = true;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    if (!voip_engine_->Base().StopSend(channel_) ||
        !voip_engine_->Base().StopPlayout(channel_)) {
      res = false;
    }
    this->voip_engine_->Base().ReleaseChannel(channel_);
  });
  return res;
}

jboolean AndroidVoipClient::StartSend(JNIEnv* env) {
  bool res;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    res = this->voip_engine_->Base().StartSend(this->channel_);
  });
  return res;
}

jboolean AndroidVoipClient::StopSend(JNIEnv* env) {
  bool res;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    res = this->voip_engine_->Base().StopSend(this->channel_);
  });
  return res;
}

jboolean AndroidVoipClient::StartPlayout(JNIEnv* env) {
  bool res;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    res = this->voip_engine_->Base().StartPlayout(this->channel_);
  });
  return res;
}

jboolean AndroidVoipClient::StopPlayout(JNIEnv* env) {
  bool res;
  rtp_socket_thread_->Invoke<void>(RTC_FROM_HERE, [this, &res] {
    res = this->voip_engine_->Base().StopPlayout(this->channel_);
  });
  return res;
}

void AndroidVoipClient::Delete(JNIEnv* env) {
  delete this;
}

bool AndroidVoipClient::SendRtp(const uint8_t* packet,
                                size_t length,
                                const webrtc::PacketOptions& options) {
  return rtp_socket_->SendTo(packet, length, remote_address_,
                             rtc::PacketOptions());
}

bool AndroidVoipClient::SendRtcp(const uint8_t* packet, size_t length) {
  return rtcp_socket_->SendTo(packet, length, remote_address_,
                              rtc::PacketOptions());
}

void AndroidVoipClient::OnSignalReadRTPPacket(rtc::AsyncPacketSocket* socket,
                                              const char* name,
                                              size_t size,
                                              const rtc::SocketAddress& addr,
                                              const int64_t& timestamp) {
  voip_engine_->Network().ReceivedRTPPacket(
      channel_, rtc::ArrayView<const uint8_t>(
                    reinterpret_cast<const unsigned char*>(name), size));
}

void AndroidVoipClient::OnSignalReadRTCPPacket(rtc::AsyncPacketSocket* socket,
                                               const char* name,
                                               size_t size,
                                               const rtc::SocketAddress& addr,
                                               const int64_t& timestamp) {
  voip_engine_->Network().ReceivedRTCPPacket(
      channel_, rtc::ArrayView<const uint8_t>(
                    reinterpret_cast<const unsigned char*>(name), size));
}

webrtc::ScopedJavaLocalRef<jobject> AndroidVoipClient::GetCodecNames(
    JNIEnv* env,
    std::vector<webrtc::AudioCodecSpec> codec_specs) {
  std::vector<std::string> names;
  for (const webrtc::AudioCodecSpec& spec : codec_specs) {
    names.push_back(spec.format.name);
  }
  webrtc::ScopedJavaLocalRef<jstring> (*convert_function)(
      JNIEnv*, const std::string&) = &webrtc::NativeToJavaString;
  return NativeToJavaList(env, names, convert_function);
}

rtc::IPAddress AndroidVoipClient::QueryDefaultLocalAddress(int family) {
  const char kPublicIPv4Host[] = "8.8.8.8";
  const char kPublicIPv6Host[] = "2001:4860:4860::8888";
  const int kPublicPort = 53;
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::CreateWithSocketServer();

  RTC_DCHECK(thread->socketserver() != nullptr);
  RTC_DCHECK(family == AF_INET || family == AF_INET6);

  std::unique_ptr<rtc::AsyncSocket> socket(
      thread->socketserver()->CreateAsyncSocket(family, SOCK_DGRAM));
  if (!socket) {
    RTC_LOG_ERR(LERROR) << "Socket creation failed";
    return rtc::IPAddress();
  }

  if (socket->Connect(rtc::SocketAddress(
          family == AF_INET ? kPublicIPv4Host : kPublicIPv6Host, kPublicPort)) <
      0) {
    if (socket->GetError() != ENETUNREACH &&
        socket->GetError() != EHOSTUNREACH) {
      RTC_LOG(LS_INFO) << "Connect failed with " << socket->GetError();
    }
    return rtc::IPAddress();
  }
  return socket->GetLocalAddress().ipaddr();
}

static jlong JNI_VoipClient_CreateClient(JNIEnv* env) {
  return webrtc::NativeToJavaPointer(new webrtc_examples::AndroidVoipClient());
}

}  // namespace webrtc_examples
