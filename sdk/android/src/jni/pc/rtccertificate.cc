/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtccertificate.h"
#include "sdk/android/src/jni/pc/icecandidate.h"

#include "rtc_base/refcount.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/rtccertificategenerator.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCCertificate_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

rtc::scoped_refptr<rtc::RTCCertificate> JavaToNativeRTCCertificate(
    JNIEnv* jni,
    const JavaRef<jobject>& j_rtc_certificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  ScopedJavaLocalRef<jstring> privatekey_field =
      Java_RTCCertificate_getPrivateKey(jni, j_rtc_certificate);
  ScopedJavaLocalRef<jstring> certificate_field =
      Java_RTCCertificate_getCertificate(jni, j_rtc_certificate);
  static const rtc::RTCCertificatePEM pem =
      rtc::RTCCertificatePEM(JavaToNativeString(jni, privatekey_field),
                             JavaToNativeString(jni, certificate_field));
  certificate = rtc::RTCCertificate::FromPEM(pem);
  return certificate;
}

static ScopedJavaLocalRef<jstring> JNI_RTCCertificate_GenerateCertificate(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& j_key_type,
    jlong j_expires) {
  rtc::KeyType key_type = JavaToNativeKeyType(jni, j_key_type);
  uint64_t expires = (uint64_t)j_expires;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificateGenerator::GenerateCertificate(
          rtc::KeyParams(key_type), expires);
  rtc::RTCCertificatePEM pem = certificate->ToPEM();
  return NativeToJavaString(jni, pem.private_key() + "|" + pem.certificate());
}

}  // namespace jni
}  // namespace webrtc
