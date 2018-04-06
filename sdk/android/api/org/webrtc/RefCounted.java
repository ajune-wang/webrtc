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
 * Interface for ref counted objects in WebRTC. These objects have significant resources that need
 * to be freed when they are no longer in use. Each objects starts with ref count of one when
 * created.
 */
public interface RefCounted {
  /** Increases ref count by one. */
  void retain();

  /**
   * Decreases ref count by one. When the ref count reaches zero, resources related to the object
   * will be freed.
   */
  void release();
}
