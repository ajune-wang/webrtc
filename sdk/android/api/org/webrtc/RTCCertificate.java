/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public class RTCCertificate {
  public final String privateKey;
  public final String certificate;

  @CalledByNative
  public RTCCertificate(String privateKey, String certificate) {
    this.privateKey = privateKey;
    this.certificate = certificate;
  }

  @CalledByNative
  String getPrivateKey() {
    return privateKey;
  }

  @CalledByNative
  String getCertificate() {
    return certificate;
  }

  public static RTCCertificate generateCertificate() {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, 100000);
  }

  public static RTCCertificate generateCertificate(PeerConnection.KeyType keyType) {
    return nativeGenerateCertificate(keyType, 100000);
  }

  public static RTCCertificate generateCertificate(long expires) {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, expires);
  }

  public static RTCCertificate generateCertificate(PeerConnection.KeyType keyType, long expires) {
    return nativeGenerateCertificate(keyType, expires);
  }

  private static native RTCCertificate nativeGenerateCertificate(
      PeerConnection.KeyType keyType, long expires);
}
