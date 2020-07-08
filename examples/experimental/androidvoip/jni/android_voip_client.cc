/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/experimental/androidvoip/jni/android_voip_client.h"

#include <errno.h>
#include <sys/socket.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_engine_factory.h"
#include "api/voip/voip_network.h"
#include "examples/experimental/androidvoip/generated_jni/VoipClient_jni.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_server.h"
#include "sdk/android/native_api/jni/java_types.h"

namespace {

// Connects a UDP socket to a public address and returns the local
// address associated with it. Since it binds to the "any" address
// internally, it returns the default local address on a multi-homed
// endpoint. Implementation copied from
// BasicNetworkManager::QueryDefaultLocalAddress.
rtc::IPAddress QueryDefaultLocalAddress(int family) {
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
          family == AF_INET ? kPublicIPv4Host :
          kPublicIPv6Host, kPublicPort)) < 0) {
    if (socket->GetError() != ENETUNREACH &&
        socket->GetError() != EHOSTUNREACH) {
      RTC_LOG(LS_INFO) << "Connect failed with " << socket->GetError();
    }
    return rtc::IPAddress();
  }
  return socket->GetLocalAddress().ipaddr();
}

// Assigned payload type for supported built-in codecs. PCMU, PCMA,
// and G722 have set payload types. Whereas opus, ISAC, and ILBC
// have dynamic payload types.
enum class PayloadType : int {
  kPcmu = 0,
  kPcma = 8,
  kG722 = 9,
  kOpus = 96,
  kIsac = 97,
  kIlbc = 98,
};

// Returns the payload type corresponding to codec_name. Only
// supports the built-in codecs.
int GetPayloadType(std::string codec_name) {
  RTC_DCHECK(codec_name == "PCMU" || codec_name == "PCMA" ||
             codec_name == "G722" || codec_name == "opus" ||
             codec_name == "ISAC" || codec_name == "ILBC");

  if (codec_name == "PCMU") {
    return static_cast<int>(PayloadType::kPcmu);
  } else if (codec_name == "PCMA") {
    return static_cast<int>(PayloadType::kPcma);
  } else if (codec_name == "G722") {
    return static_cast<int>(PayloadType::kG722);
  } else if (codec_name == "opus") {
    return static_cast<int>(PayloadType::kOpus);
  } else if (codec_name == "ISAC") {
    return static_cast<int>(PayloadType::kIsac);
  } else if (codec_name == "ILBC") {
    return static_cast<int>(PayloadType::kIlbc);
  }

  RTC_NOTREACHED();
  return -1;
}

}  // namespace

namespace webrtc_examples {

AndroidVoipClient::AndroidVoipClient() {
  voip_thread_ = rtc::Thread::CreateWithSocketServer();
  voip_thread_->Start();

  voip_thread_->Invoke<void>(RTC_FROM_HERE, [this] {
    webrtc::VoipEngineConfig config;
    config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    config.decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    config.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();

    config.audio_device_module = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kPlatformDefaultAudio,
        config.task_queue_factory.get());
    if (!config.audio_device_module) {
      RTC_LOG(LS_ERROR) << "AudioDeviceModule creation failed";
      return;
    }

    config.audio_processing = webrtc::AudioProcessingBuilder().Create();
    if (!config.audio_processing) {
      RTC_LOG(LS_ERROR) << "AudioProcessing creation failed";
      return;
    }

    supported_codecs_ = config.encoder_factory->GetSupportedEncoders();

    voip_engine_ = webrtc::CreateVoipEngine(std::move(config));
    if (!voip_engine_) {
      RTC_LOG(LS_ERROR) << "VoipEngine creation failed";
    }
  });
}

AndroidVoipClient::~AndroidVoipClient() {
  voip_thread_->Stop();
}

AndroidVoipClient* AndroidVoipClient::Create() {
  std::unique_ptr<AndroidVoipClient> voip_client(new AndroidVoipClient());
  if (!voip_client->voip_engine_) {
    return nullptr;
  }
  return voip_client.release();
}

webrtc::ScopedJavaLocalRef<jobject> AndroidVoipClient::GetSupportedCodecs(
    JNIEnv* env) {
  std::vector<std::string> names;
  for (const webrtc::AudioCodecSpec& spec : supported_codecs_) {
    names.push_back(spec.format.name);
  }
  webrtc::ScopedJavaLocalRef<jstring> (*convert_function)(
      JNIEnv*, const std::string&) = &webrtc::NativeToJavaString;
  return NativeToJavaList(env, names, convert_function);
}

webrtc::ScopedJavaLocalRef<jstring> AndroidVoipClient::GetLocalIPAddress(
    JNIEnv* env) {
  rtc::IPAddress ipv4_address = QueryDefaultLocalAddress(AF_INET);
  if (!ipv4_address.IsNil()) {
    return webrtc::NativeToJavaString(env, ipv4_address.ToString());
  }
  rtc::IPAddress ipv6_address = QueryDefaultLocalAddress(AF_INET6);
  if (!ipv6_address.IsNil()) {
    return webrtc::NativeToJavaString(env, ipv6_address.ToString());
  }
  return webrtc::NativeToJavaString(env, "");
}

void AndroidVoipClient::SetEncoder(
    JNIEnv* env,
    const webrtc::JavaRef<jstring>& j_encoder_string) {
  const std::string& chosen_encoder =
      webrtc::JavaToNativeString(env, j_encoder_string);
  for (const webrtc::AudioCodecSpec& encoder : supported_codecs_) {
    if (encoder.format.name == chosen_encoder) {
      voip_engine_->Codec().SetSendCodec(
          channel_, GetPayloadType(encoder.format.name), encoder.format);
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

  for (const webrtc::AudioCodecSpec& decoder : supported_codecs_) {
    if (decoder_strings_set.count(decoder.format.name)) {
      decoder_specs.insert(
          {GetPayloadType(decoder.format.name), decoder.format});
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
  rtp_remote_address_ = rtc::SocketAddress(ip_address, j_port_number_int);
  rtcp_remote_address_ = rtc::SocketAddress(ip_address, j_port_number_int + 1);
}

jboolean AndroidVoipClient::StartSession(JNIEnv* env) {
  auto channel = voip_thread_->Invoke<absl::optional<webrtc::ChannelId>>(
      RTC_FROM_HERE,
      [this] { return voip_engine_->Base().CreateChannel(this, 0); });
  if (!channel) {
    RTC_LOG(LS_ERROR) << "Channel creation failed";
    return false;
  }
  channel_ = *channel;

  if (!rtp_socket_) {
    rtp_socket_.reset(rtc::AsyncUDPSocket::Create(voip_thread_->socketserver(),
                                                  rtp_local_address_));
    if (!rtp_socket_) {
      RTC_LOG_ERR(LERROR) << "Socket creation failed";
      return false;
    }
    rtp_socket_->SignalReadPacket.connect(
        this, &AndroidVoipClient::OnSignalReadRTPPacket);
  }

  if (!rtcp_socket_) {
    rtcp_socket_.reset(rtc::AsyncUDPSocket::Create(voip_thread_->socketserver(),
                                                   rtcp_local_address_));
    if (!rtcp_socket_) {
      RTC_LOG_ERR(LERROR) << "Socket creation failed";
      return false;
    }
    rtcp_socket_->SignalReadPacket.connect(
        this, &AndroidVoipClient::OnSignalReadRTCPPacket);
  }
  return true;
}

jboolean AndroidVoipClient::StopSession(JNIEnv* env) {
  return voip_thread_->Invoke<bool>(RTC_FROM_HERE, [this] {
    if (!voip_engine_->Base().StopSend(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to stop send";
      return false;
    }
    if (!voip_engine_->Base().StopPlayout(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to stop playout";
      return false;
    }
    voip_engine_->Base().ReleaseChannel(channel_);
    return true;
  });
}

jboolean AndroidVoipClient::StartSend(JNIEnv* env) {
  return voip_thread_->Invoke<bool>(RTC_FROM_HERE, [this] {
    if (!voip_engine_->Base().StartSend(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to start send";
      return false;
    }
    return true;
  });
}

jboolean AndroidVoipClient::StopSend(JNIEnv* env) {
  return voip_thread_->Invoke<bool>(RTC_FROM_HERE, [this] {
    if (!voip_engine_->Base().StopSend(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to stop send";
      return false;
    }
    return true;
  });
}

jboolean AndroidVoipClient::StartPlayout(JNIEnv* env) {
  return voip_thread_->Invoke<bool>(RTC_FROM_HERE, [this] {
    if (!voip_engine_->Base().StartPlayout(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to start playout";
      return false;
    }
    return true;
  });
}

jboolean AndroidVoipClient::StopPlayout(JNIEnv* env) {
  return voip_thread_->Invoke<bool>(RTC_FROM_HERE, [this] {
    if (!voip_engine_->Base().StopPlayout(channel_)) {
      RTC_LOG(LS_ERROR) << "Failed to stop playout";
      return false;
    }
    return true;
  });
}

void AndroidVoipClient::Delete(JNIEnv* env) {
  delete this;
}

bool AndroidVoipClient::SendRtp(const uint8_t* packet,
                                size_t length,
                                const webrtc::PacketOptions& options) {
  if (!rtp_socket_->SendTo(packet, length, rtp_remote_address_,
                           rtc::PacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTP packet";
    return false;
  }
  return true;
}

bool AndroidVoipClient::SendRtcp(const uint8_t* packet, size_t length) {
  if (!rtcp_socket_->SendTo(packet, length, rtcp_remote_address_,
                            rtc::PacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTCP packet";
    return false;
  }
  return true;
}

void AndroidVoipClient::OnSignalReadRTPPacket(rtc::AsyncPacketSocket* socket,
                                              const char* rtp_packet,
                                              size_t size,
                                              const rtc::SocketAddress& addr,
                                              const int64_t& timestamp) {
  voip_engine_->Network().ReceivedRTPPacket(
      channel_, rtc::ArrayView<const uint8_t>(
                    reinterpret_cast<const unsigned char*>(rtp_packet), size));
}

void AndroidVoipClient::OnSignalReadRTCPPacket(rtc::AsyncPacketSocket* socket,
                                               const char* rtcp_packet,
                                               size_t size,
                                               const rtc::SocketAddress& addr,
                                               const int64_t& timestamp) {
  voip_engine_->Network().ReceivedRTCPPacket(
      channel_, rtc::ArrayView<const uint8_t>(
                    reinterpret_cast<const unsigned char*>(rtcp_packet), size));
}

static jlong JNI_VoipClient_CreateClient(JNIEnv* env) {
  return webrtc::NativeToJavaPointer(AndroidVoipClient::Create());
}

}  // namespace webrtc_examples
