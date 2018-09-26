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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import android.support.test.filters.SmallTest;
import org.junit.runner.RunWith;
import org.junit.Test;
import org.webrtc.RTCCertificate;
import org.webrtc.PeerConnection.KeyType;

/** Tests for RTCCertificate.java. */
@RunWith(BaseJUnit4ClassRunner.class)
public class RTCCertificateTest {
  @Test
  @SmallTest
  public void testConstructor() {
    RTCCertificate original = RTCCertificate.generateCertificate();
    assertNotNull(original.privateKey);
    assertNotNull(original.certificate);
    assertNotEquals(original.privateKey, "");
    assertNotEquals(original.certificate, "");

    RTCCertificate recreated = new RTCCertificate(original.privateKey, original.certificate);
    assertNotNull(recreated.privateKey);
    assertNotNull(recreated.certificate);
    assertNotEquals(recreated.privateKey, "");
    assertNotEquals(recreated.certificate, "");

    assertEquals(original.privateKey, recreated.privateKey);
    assertEquals(original.certificate, recreated.certificate);
  }

  @Test
  @SmallTest
  public void testGenerateCertificateDefaults() {
    RTCCertificate rtcCertificate = RTCCertificate.generateCertificate();
    assertNotNull(rtcCertificate.privateKey);
    assertNotNull(rtcCertificate.certificate);
    assertNotEquals(rtcCertificate.privateKey, "");
    assertNotEquals(rtcCertificate.certificate, "");
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomKeyTypeDefaultExpires() {
    RTCCertificate rtcCertificate = RTCCertificate.generateCertificate(PeerConnection.KeyType.RSA);
    assertNotNull(rtcCertificate.privateKey);
    assertNotNull(rtcCertificate.certificate);
    assertNotEquals(rtcCertificate.privateKey, "");
    assertNotEquals(rtcCertificate.certificate, "");
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomExpiresDefaultKeyType() {
    RTCCertificate rtcCertificate = RTCCertificate.generateCertificate(60 * 60 * 24);
    assertNotNull(rtcCertificate.privateKey);
    assertNotNull(rtcCertificate.certificate);
    assertNotEquals(rtcCertificate.privateKey, "");
    assertNotEquals(rtcCertificate.certificate, "");
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomKeyTypeAndExpires() {
    RTCCertificate rtcCertificate =
        RTCCertificate.generateCertificate(PeerConnection.KeyType.RSA, 60 * 60 * 24);
    assertNotNull(rtcCertificate.privateKey);
    assertNotNull(rtcCertificate.certificate);
    assertNotEquals(rtcCertificate.privateKey, "");
    assertNotEquals(rtcCertificate.certificate, "");
  }
}
