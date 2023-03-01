/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

// This file is a fork of chromium
// base/android/java/src/org/chromium/base/annotations/NativeMethods.java file.
// The fork is needed to allow usage of NativeMethods in some upstream
// projects.

@Target(ElementType.TYPE)
@Retention(RetentionPolicy.SOURCE)
public @interface NativeMethods {
  /**
   * Tells the build system to call a different GEN_JNI, prefixed by the value we put here. This
   * should only be used for feature modules where we need a different GEN_JNI. For example, if
   * you did @NativeMethods("dfmname"), this would call into dfmname_GEN_JNI.java.
   */
  public String value() default "";
}