/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/java_native_conversion.h"

#include <string>

#include "pc/webrtcsdp.h"
#include "sdk/android/generated_peerconnection_jni/jni/IceCandidate_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaStreamTrack_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RtpParameters_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/SessionDescription_jni.h"

namespace webrtc {
namespace jni {

namespace {

jobject CreateJavaIceCandidate(JNIEnv* env,
                               const std::string& sdp_mid,
                               int sdp_mline_index,
                               const std::string& sdp,
                               const std::string server_url) {
  return Java_IceCandidate_Constructor(
      env, NativeToJavaString(env, sdp_mid), sdp_mline_index,
      NativeToJavaString(env, sdp), NativeToJavaString(env, server_url));
}

jobject NativeToJavaRtpEncodingParameter(
    JNIEnv* env,
    const RtpEncodingParameters& encoding) {
  return Java_Encoding_Constructor(
      env, encoding.active, NativeToJavaInteger(env, encoding.max_bitrate_bps),
      encoding.ssrc ? NativeToJavaLong(env, *encoding.ssrc) : nullptr);
}

jobject NativeToJavaRtpCodecParameter(JNIEnv* env,
                                      const RtpCodecParameters& codec) {
  return Java_Codec_Constructor(env, codec.payload_type,
                                NativeToJavaString(env, codec.name),
                                NativeToJavaMediaType(env, codec.kind),
                                NativeToJavaInteger(env, codec.clock_rate),
                                NativeToJavaInteger(env, codec.num_channels));
}

}  // namespace

jobject NativeToJavaMediaType(JNIEnv* jni, cricket::MediaType media_type) {
  return Java_MediaType_fromNativeIndex(jni, media_type);
}

jobject NativeToJavaMediaTrackState(
    JNIEnv* env,
    MediaStreamTrackInterface::TrackState state) {
  return Java_State_fromNativeIndex(env, state);
}

cricket::MediaType JavaToNativeMediaType(JNIEnv* jni, jobject j_media_type) {
  return static_cast<cricket::MediaType>(
      Java_MediaType_getNative(jni, j_media_type));
}

cricket::Candidate JavaToNativeCandidate(JNIEnv* jni, jobject j_candidate) {
  std::string sdp_mid =
      JavaToStdString(jni, Java_IceCandidate_getSdpMid(jni, j_candidate));
  std::string sdp =
      JavaToStdString(jni, Java_IceCandidate_getSdp(jni, j_candidate));
  cricket::Candidate candidate;
  if (!SdpDeserializeCandidate(sdp_mid, sdp, &candidate, NULL)) {
    RTC_LOG(LS_ERROR) << "SdpDescrializeCandidate failed with sdp " << sdp;
  }
  return candidate;
}

jobject NativeToJavaCandidate(JNIEnv* env,
                              const cricket::Candidate& candidate) {
  std::string sdp = SdpSerializeCandidate(candidate);
  RTC_CHECK(!sdp.empty()) << "got an empty ICE candidate";
  // sdp_mline_index is not used, pass an invalid value -1.
  return CreateJavaIceCandidate(env, candidate.transport_name(),
                                -1 /* sdp_mline_index */, sdp,
                                "" /* server_url */);
}

jobject NativeToJavaIceCandidate(JNIEnv* env,
                                 const IceCandidateInterface& candidate) {
  std::string sdp;
  RTC_CHECK(candidate.ToString(&sdp)) << "got so far: " << sdp;
  return CreateJavaIceCandidate(env, candidate.sdp_mid(),
                                candidate.sdp_mline_index(), sdp,
                                candidate.candidate().url());
}

jobjectArray NativeToJavaCandidateArray(
    JNIEnv* jni,
    const std::vector<cricket::Candidate>& candidates) {
  return NativeToJavaObjectArray(jni, candidates,
                                 org_webrtc_IceCandidate_clazz(jni),
                                 &NativeToJavaCandidate);
}

std::unique_ptr<SessionDescriptionInterface> JavaToNativeSessionDescription(
    JNIEnv* jni,
    jobject j_sdp) {
  std::string std_type = JavaToStdString(
      jni, Java_SessionDescription_getTypeInCanonicalForm(jni, j_sdp));
  std::string std_description =
      JavaToStdString(jni, Java_SessionDescription_getDescription(jni, j_sdp));
  rtc::Optional<SdpType> sdp_type_maybe = SdpTypeFromString(std_type);
  if (!sdp_type_maybe) {
    RTC_LOG(LS_ERROR) << "Unexpected SDP type: " << std_type;
    return nullptr;
  }
  return CreateSessionDescription(*sdp_type_maybe, std_description);
}

jobject NativeToJavaSessionDescription(
    JNIEnv* jni,
    const SessionDescriptionInterface* desc) {
  std::string sdp;
  RTC_CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = NativeToJavaString(jni, sdp);
  jobject j_type =
      Java_Type_fromCanonicalForm(jni, NativeToJavaString(jni, desc->type()));
  jobject j_sdp =
      Java_SessionDescription_Constructor(jni, j_type, j_description);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_sdp;
}

PeerConnectionInterface::IceTransportsType JavaToNativeIceTransportsType(
    JNIEnv* jni,
    jobject j_ice_transports_type) {
  std::string enum_name = GetJavaEnumName(jni, j_ice_transports_type);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kAll;

  if (enum_name == "RELAY")
    return PeerConnectionInterface::kRelay;

  if (enum_name == "NOHOST")
    return PeerConnectionInterface::kNoHost;

  if (enum_name == "NONE")
    return PeerConnectionInterface::kNone;

  RTC_CHECK(false) << "Unexpected IceTransportsType enum_name " << enum_name;
  return PeerConnectionInterface::kAll;
}

PeerConnectionInterface::BundlePolicy JavaToNativeBundlePolicy(
    JNIEnv* jni,
    jobject j_bundle_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_bundle_policy);

  if (enum_name == "BALANCED")
    return PeerConnectionInterface::kBundlePolicyBalanced;

  if (enum_name == "MAXBUNDLE")
    return PeerConnectionInterface::kBundlePolicyMaxBundle;

  if (enum_name == "MAXCOMPAT")
    return PeerConnectionInterface::kBundlePolicyMaxCompat;

  RTC_CHECK(false) << "Unexpected BundlePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kBundlePolicyBalanced;
}

PeerConnectionInterface::RtcpMuxPolicy JavaToNativeRtcpMuxPolicy(
    JNIEnv* jni,
    jobject j_rtcp_mux_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_rtcp_mux_policy);

  if (enum_name == "NEGOTIATE")
    return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;

  if (enum_name == "REQUIRE")
    return PeerConnectionInterface::kRtcpMuxPolicyRequire;

  RTC_CHECK(false) << "Unexpected RtcpMuxPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
}

PeerConnectionInterface::TcpCandidatePolicy JavaToNativeTcpCandidatePolicy(
    JNIEnv* jni,
    jobject j_tcp_candidate_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_tcp_candidate_policy);

  if (enum_name == "ENABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyEnabled;

  if (enum_name == "DISABLED")
    return PeerConnectionInterface::kTcpCandidatePolicyDisabled;

  RTC_CHECK(false) << "Unexpected TcpCandidatePolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTcpCandidatePolicyEnabled;
}

PeerConnectionInterface::CandidateNetworkPolicy
JavaToNativeCandidateNetworkPolicy(JNIEnv* jni,
                                   jobject j_candidate_network_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_candidate_network_policy);

  if (enum_name == "ALL")
    return PeerConnectionInterface::kCandidateNetworkPolicyAll;

  if (enum_name == "LOW_COST")
    return PeerConnectionInterface::kCandidateNetworkPolicyLowCost;

  RTC_CHECK(false) << "Unexpected CandidateNetworkPolicy enum_name "
                   << enum_name;
  return PeerConnectionInterface::kCandidateNetworkPolicyAll;
}

rtc::KeyType JavaToNativeKeyType(JNIEnv* jni, jobject j_key_type) {
  std::string enum_name = GetJavaEnumName(jni, j_key_type);

  if (enum_name == "RSA")
    return rtc::KT_RSA;
  if (enum_name == "ECDSA")
    return rtc::KT_ECDSA;

  RTC_CHECK(false) << "Unexpected KeyType enum_name " << enum_name;
  return rtc::KT_ECDSA;
}

PeerConnectionInterface::ContinualGatheringPolicy
JavaToNativeContinualGatheringPolicy(JNIEnv* jni, jobject j_gathering_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_gathering_policy);
  if (enum_name == "GATHER_ONCE")
    return PeerConnectionInterface::GATHER_ONCE;

  if (enum_name == "GATHER_CONTINUALLY")
    return PeerConnectionInterface::GATHER_CONTINUALLY;

  RTC_CHECK(false) << "Unexpected ContinualGatheringPolicy enum name "
                   << enum_name;
  return PeerConnectionInterface::GATHER_ONCE;
}

PeerConnectionInterface::TlsCertPolicy JavaToNativeTlsCertPolicy(
    JNIEnv* jni,
    jobject j_ice_server_tls_cert_policy) {
  std::string enum_name = GetJavaEnumName(jni, j_ice_server_tls_cert_policy);

  if (enum_name == "TLS_CERT_POLICY_SECURE")
    return PeerConnectionInterface::kTlsCertPolicySecure;

  if (enum_name == "TLS_CERT_POLICY_INSECURE_NO_CHECK")
    return PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;

  RTC_CHECK(false) << "Unexpected TlsCertPolicy enum_name " << enum_name;
  return PeerConnectionInterface::kTlsCertPolicySecure;
}

RtpParameters JavaToNativeRtpParameters(JNIEnv* jni, jobject j_parameters) {
  RtpParameters parameters;

  // Convert encodings.
  jobject j_encodings = Java_RtpParameters_getEncodings(jni, j_parameters);
  for (jobject j_encoding_parameters : Iterable(jni, j_encodings)) {
    RtpEncodingParameters encoding;
    encoding.active = Java_Encoding_getActive(jni, j_encoding_parameters);
    jobject j_bitrate =
        Java_Encoding_getMaxBitrateBps(jni, j_encoding_parameters);
    encoding.max_bitrate_bps = JavaToNativeOptionalInt(jni, j_bitrate);
    jobject j_ssrc = Java_Encoding_getSsrc(jni, j_encoding_parameters);
    if (!IsNull(jni, j_ssrc))
      encoding.ssrc = JavaToNativeLong(jni, j_ssrc);
    parameters.encodings.push_back(encoding);
  }

  // Convert codecs.
  jobject j_codecs = Java_RtpParameters_getCodecs(jni, j_parameters);
  for (jobject j_codec : Iterable(jni, j_codecs)) {
    RtpCodecParameters codec;
    codec.payload_type = Java_Codec_getPayloadType(jni, j_codec);
    codec.name = JavaToStdString(jni, Java_Codec_getName(jni, j_codec));
    codec.kind = JavaToNativeMediaType(jni, Java_Codec_getKind(jni, j_codec));
    codec.clock_rate =
        JavaToNativeOptionalInt(jni, Java_Codec_getClockRate(jni, j_codec));
    codec.num_channels =
        JavaToNativeOptionalInt(jni, Java_Codec_getNumChannels(jni, j_codec));
    parameters.codecs.push_back(codec);
  }
  return parameters;
}

jobject NativeToJavaRtpParameters(JNIEnv* env,
                                  const RtpParameters& parameters) {
  return Java_RtpParameters_Constructor(
      env,
      NativeToJavaList(env, parameters.encodings,
                       &NativeToJavaRtpEncodingParameter),
      NativeToJavaList(env, parameters.codecs, &NativeToJavaRtpCodecParameter));
}

}  // namespace jni
}  // namespace webrtc
