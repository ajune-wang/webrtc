/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.HandlerThread;
import androidx.annotation.Nullable;
import org.webrtc.EglBase.EglConnection;

/** EGL graphics thread that allows multiple clients to share the same underlying EGLContext. */
public class EglThread {
  /** Callback for externally managed reference count. */
  public interface ReleaseMonitor {
    /**
     * Called by EglThread when a client releases its reference. Returns true when there are no more
     * references and resources should be released.
     */
    boolean onRelease(EglThread eglThread);
  }

  private final ReleaseMonitor releaseMonitor;
  private final Object handlerLock = new Object();
  private Handler handler;
  private EglConnection eglConnection;

  public EglThread(@Nullable ReleaseMonitor releaseMonitor) {
    this.releaseMonitor = releaseMonitor != null ? releaseMonitor : eglThread -> true;
  }

  public void init(@Nullable final EglBase.Context sharedContext, final int[] configAttributes) {
    synchronized (handlerLock) {
      if (handler != null) {
        // Already initialized.
        return;
      }

      final HandlerThread renderThread = new HandlerThread("EglThread");
      renderThread.start();
      handler = new Handler(renderThread.getLooper());

      // Create EGL context on the newly created render thread. It should be possibly to create the
      // context on this thread and make it current on the render thread, but this causes failure on
      // some Marvel based JB devices. https://bugs.chromium.org/p/webrtc/issues/detail?id=6350.
      ThreadUtils.invokeAtFrontUninterruptibly(handler, () -> {
        // If sharedContext is null, then texture frames are disabled. This is typically for old
        // devices that might not be fully spec compliant, so force EGL 1.0 since EGL 1.4 has
        // caused trouble on some weird devices.
        if (sharedContext == null) {
          eglConnection = EglConnection.createEgl10(configAttributes);
        } else {
          eglConnection = EglConnection.create(sharedContext, configAttributes);
        }
      });
    }
  }

  public void release() {
    if (!releaseMonitor.onRelease(this)) {
      // Thread is still in use, do not release yet.
      return;
    }

    synchronized (handlerLock) {
      if (handler == null) {
        // Never initialized.
        return;
      }

      handler.post(() -> {
        eglConnection.release();
        eglConnection = null;
      });

      handler.getLooper().quitSafely();
      handler = null;
    }
  }

  public EglBase createEglBaseWithSharedConnection() {
    return EglBase.create(eglConnection);
  }

  public Handler getHandler() {
    return handler;
  }
}
