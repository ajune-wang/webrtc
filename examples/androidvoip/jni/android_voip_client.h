/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_
#define EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "audio/voip/voip_core.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc_examples {

class AndroidVoipClient : public webrtc::Transport,
                          public sigslot::has_slots<> {
 public:
  AndroidVoipClient();
  ~AndroidVoipClient();

  // A separate method for class constructor logic so a boolean
  // can be returned in case of errors
  bool Initialize(JNIEnv* env);

  // Getters called by client to populate UI
  webrtc::ScopedJavaLocalRef<jobject> GetSupportedEncoders(JNIEnv* env);
  webrtc::ScopedJavaLocalRef<jobject> GetSupportedDecoders(JNIEnv* env);
  webrtc::ScopedJavaLocalRef<jstring> GetLocalIPAddress(JNIEnv* env);

  // Setters for user defined parameters
  void SetEncoder(JNIEnv* env,
                  const webrtc::JavaRef<jstring>& j_encoder_string);
  void SetDecoders(JNIEnv* env,
                   const webrtc::JavaParamRef<jobject>& j_decoder_strings);
  void SetLocalAddress(JNIEnv* env,
                       const webrtc::JavaRef<jstring>& j_ip_address_string,
                       jint j_port_number_int);
  void SetRemoteAddress(JNIEnv* env,
                        const webrtc::JavaRef<jstring>& j_ip_address_string,
                        jint j_port_number_int);

  // VoIP API related methods
  jboolean StartSession(JNIEnv* env);
  jboolean StopSession(JNIEnv* env);
  jboolean StartSend(JNIEnv* env);
  jboolean StopSend(JNIEnv* env);
  jboolean StartPlayout(JNIEnv* env);
  jboolean StopPlayout(JNIEnv* env);

  void Delete(JNIEnv* env);

  // Implementation for Transport
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const webrtc::PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  // Implementation for has_slots
  void OnSignalReadRTPPacket(rtc::AsyncPacketSocket* socket,
                             const char* name,
                             size_t size,
                             const rtc::SocketAddress& addr,
                             const int64_t& timestamp);
  void OnSignalReadRTCPPacket(rtc::AsyncPacketSocket* socket,
                              const char* name,
                              size_t size,
                              const rtc::SocketAddress& addr,
                              const int64_t& timestamp);

 private:
  std::unique_ptr<rtc::Thread> rtp_socket_thread_;
  std::unique_ptr<rtc::Thread> rtcp_socket_thread_;
  std::vector<webrtc::AudioCodecSpec> supported_encoders_;
  std::vector<webrtc::AudioCodecSpec> supported_decoders_;
  std::unique_ptr<webrtc::VoipEngine> voip_engine_;
  webrtc::ChannelId channel_;
  std::unique_ptr<rtc::AsyncUDPSocket> rtp_socket_;
  std::unique_ptr<rtc::AsyncUDPSocket> rtcp_socket_;
  rtc::SocketAddress rtp_local_address_;
  rtc::SocketAddress rtcp_local_address_;
  rtc::SocketAddress remote_address_;

  webrtc::ScopedJavaLocalRef<jobject> GetCodecNames(
      JNIEnv* env,
      std::vector<webrtc::AudioCodecSpec> codec_specs);
  // Helper method for fetching local IP address
  static rtc::IPAddress QueryDefaultLocalAddress(int family);
};

}  // namespace webrtc_examples

#endif  // EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_
