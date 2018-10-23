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
 * Java version of webrtc::CryptoOptions API.
 */
public class CryptoOptions {
  /**
   * SRTP Related Peer Connection Options.
   */
  public class Srtp {
    /**
     * Enable GCM crypto suites from RFC 7714 for SRTP. GCM will only be used
     * if both sides enable it
     */
    boolean enableGcmCryptoSuites = false;
    /**
     * If set to true, the (potentially insecure) crypto cipher
     * SRTP_AES128_CM_SHA1_32 will be included in the list of supported ciphers
     * during negotiation. It will only be used if both peers support it and no
     * other ciphers get preferred.
     */
    boolean enableAes128Sha1_32CryptoCipher = false;
    /**
     * If set to true, encrypted RTP header extensions as defined in RFC 6904
     * will be negotiated. They will only be used if both peers support them.
     */
    boolean enableEncryptedRtpHeaderExtensions = false;

    @CalledByNative("CryptoOptions_Srtp")
    boolean getEnableGcmCryptoSuites() {
      return enableGcmCryptoSuites;
    }

    @CalledByNative("CryptoOptions_Srtp")
    boolean getEnableAes128Sha1_32CryptoCipher() {
      return enableAes128Sha1_32CryptoCipher;
    }

    @CalledByNative("CryptoOptions_Srtp")
    boolean getEnableEncryptedRtpHeaderExtensions() {
      return enableEncryptedRtpHeaderExtensions;
    }
  }

  /**
   * Options to be used when the FrameEncryptor / FrameDecryptor APIs are used.
   */
  public class SFrame {
    /**
     * If set all RtpSenders must have an FrameEncryptor attached to them before
     * they are allowed to send packets. All RtpReceivers must have a
     * FrameDecryptor attached to them before they are able to receive packets.
     */
    boolean requireFrameEncryption = false;

    @CalledByNative("CryptoOptions_SFrame")
    boolean getRequireFrameEncryption() {
      return requireFrameEncryption;
    }
  }

  Srtp srtp = new Srtp();
  SFrame sframe = new SFrame();

  @CalledByNative
  Srtp getSrtp() {
    return srtp;
  }

  @CalledByNative
  SFrame getSFrame() {
    return sframe;
  }
}
