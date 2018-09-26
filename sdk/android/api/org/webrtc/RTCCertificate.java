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

/**
 * Easily storable/serializable version of a native C++ RTCCertificate.
 */
public class RTCCertificate {
  /** PEM string representation of the private key. */
  public final String privateKey;
  /** PEM string representation of the certificate. */
  public final String certificate;
  /** Instantiate an RTCCertificate object from stored strings. */
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

  /** Generate a new RTCCertificate with the default settings of KeyType = ECDSA and
   * expires = 30 days. */
  public static RTCCertificate generateCertificate() {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, 60 * 60 * 24 * 30);
  }

  /** Generate a new RTCCertificate with a custom KeyType and the default setting of
   * expires = 30 days. */
  public static RTCCertificate generateCertificate(PeerConnection.KeyType keyType) {
    return nativeGenerateCertificate(keyType, 60 * 60 * 24 * 30);
  }

  /** Generate a new RTCCertificate with a custom expires and the default setting of
   * KeyType = ECDSA. */
  public static RTCCertificate generateCertificate(long expires) {
    return nativeGenerateCertificate(PeerConnection.KeyType.ECDSA, expires);
  }

  /** Generate a new RTCCertificate with a custom KeyType and a custom expires. */
  public static RTCCertificate generateCertificate(PeerConnection.KeyType keyType, long expires) {
    return nativeGenerateCertificate(keyType, expires);
  }

  private static native RTCCertificate nativeGenerateCertificate(
      PeerConnection.KeyType keyType, long expires);
}
