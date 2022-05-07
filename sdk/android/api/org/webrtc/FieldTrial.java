/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public class FieldTrial {
  interface FieldTrialProvider {
    String fieldTrialsFindFullName(String name);
  }

  private static FieldTrialProvider provider;

  static void setFieldTrialProvider(FieldTrialProvider provider) {
    FieldTrial.provider = provider;
  }

  // Wrapper of webrtc::field_trial::FindFullName. Develop the feature with default behaviour off.
  // Example usage:
  // if (FieldTrial.fieldTrialsFindFullName("WebRTCExperiment").equals("Enabled")) {
  //   method1();
  // } else {
  //   method2();
  // }
  public static String fieldTrialsFindFullName(String name) {
    return (FieldTrial.provider != null) ? FieldTrial.provider.fieldTrialsFindFullName(name) : "";
  }
}
