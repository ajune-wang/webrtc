/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.webrtc.CryptoOptions;

@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CryptoOptionsTest {
  // Validates the builder builds by default all false options.
  @Test
  public void testBuilderDefaultsAreFalse() {
    CryptoOptions cryptoOptions0 = CryptoOptions.builder().createCryptoOptions();
    assertFalse(cryptoOptions0.getSrtp().getEnableGcmCryptoSuites());
    assertFalse(cryptoOptions0.getSrtp().getEnableAes128Sha1_32CryptoCipher());
    assertFalse(cryptoOptions0.getSrtp().getEnableEncryptedRtpHeaderExtensions());
    assertFalse(cryptoOptions0.getSFrame().getRequireFrameEncryption());
  }

  // Validates the builder sets the correct parameters.
  @Test
  public void testBuilderCorrectlyInitializingOptions() {
    CryptoOptions cryptoOptions0 =
        CryptoOptions.builder().setEnableGcmCryptoSuites(true).createCryptoOptions();
    assertTrue(cryptoOptions0.getSrtp().getEnableGcmCryptoSuites());
    assertFalse(cryptoOptions0.getSrtp().getEnableAes128Sha1_32CryptoCipher());
    assertFalse(cryptoOptions0.getSrtp().getEnableEncryptedRtpHeaderExtensions());
    assertFalse(cryptoOptions0.getSFrame().getRequireFrameEncryption());

    CryptoOptions cryptoOptions1 =
        CryptoOptions.builder().setEnableAes128Sha1_32CryptoCipher(true).createCryptoOptions();
    assertFalse(cryptoOptions1.getSrtp().getEnableGcmCryptoSuites());
    assertTrue(cryptoOptions1.getSrtp().getEnableAes128Sha1_32CryptoCipher());
    assertFalse(cryptoOptions1.getSrtp().getEnableEncryptedRtpHeaderExtensions());
    assertFalse(cryptoOptions1.getSFrame().getRequireFrameEncryption());

    CryptoOptions cryptoOptions2 =
        CryptoOptions.builder().setEnableEncryptedRtpHeaderExtensions(true).createCryptoOptions();
    assertFalse(cryptoOptions2.getSrtp().getEnableGcmCryptoSuites());
    assertFalse(cryptoOptions2.getSrtp().getEnableAes128Sha1_32CryptoCipher());
    assertTrue(cryptoOptions2.getSrtp().getEnableEncryptedRtpHeaderExtensions());
    assertFalse(cryptoOptions2.getSFrame().getRequireFrameEncryption());

    CryptoOptions cryptoOptions3 =
        CryptoOptions.builder().setRequireFrameEncryption(true).createCryptoOptions();
    assertFalse(cryptoOptions3.getSrtp().getEnableGcmCryptoSuites());
    assertFalse(cryptoOptions3.getSrtp().getEnableAes128Sha1_32CryptoCipher());
    assertFalse(cryptoOptions3.getSrtp().getEnableEncryptedRtpHeaderExtensions());
    assertTrue(cryptoOptions3.getSFrame().getRequireFrameEncryption());
  }
}
