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

  public RTCCertificate(String pemPair) {
    String[] pieces = pemPair.split("\\|");
    this.privateKey = pieces[0];
    this.certificate = pieces[1];
  }

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
}
