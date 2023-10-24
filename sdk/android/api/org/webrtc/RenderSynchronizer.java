/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.os.Handler;
import android.os.Looper;
import android.view.Choreographer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/**
 * Class to synchronize rendering updates with display refresh cycles and save power by blocking
 * updates that exceeds the target frame rate.
 */
public final class RenderSynchronizer {

  /** Interface for listening to render window updates. */
  public interface Listener {
    void onRenderWindowOpen();

    void onRenderWindowClose();
  }

  private static final String TAG = "RenderSynchronizer";
  private static final float DEFAULT_TARGET_FPS = 30f;
  private final Object lock = new Object();
  private final List<Listener> listeners = new ArrayList<>();
  private final long targetFrameIntervalNanos;
  private final Handler mainThreadHandler;
  private Choreographer choreographer;
  private boolean isListening = false;
  private boolean renderWindowOpen = false;
  private long lastRefreshTimeNanos;
  private long lastOpenedTimeNanos;

  public RenderSynchronizer(float targetFrameRateFps) {
    this.targetFrameIntervalNanos = Math.round(TimeUnit.SECONDS.toNanos(1) / targetFrameRateFps);
    this.mainThreadHandler = new Handler(Looper.getMainLooper());
    mainThreadHandler.post(() -> this.choreographer = Choreographer.getInstance());
    Logging.d(TAG, "Created");
  }

  public RenderSynchronizer() {
    this(DEFAULT_TARGET_FPS);
  }

  public void registerListener(Listener listener) {
    synchronized (lock) {
      listeners.add(listener);
      if (!isListening) {
        Logging.d(TAG, "First listener, subscribing to frame callbacks");
        isListening = true;
        mainThreadHandler.post(
            () -> choreographer.postFrameCallback(this::onDisplayRefreshCycleBegin));
      }
    }
  }

  public void removeListener(Listener listener) {
    synchronized (lock) {
      listeners.remove(listener);
    }
  }

  private void onDisplayRefreshCycleBegin(long refreshTimeNanos) {
    synchronized (lock) {
      if (listeners.isEmpty()) {
        Logging.d(TAG, "No listeners, unsubscribing to frame callbacks");
        isListening = false;
        return;
      }
    }
    choreographer.postFrameCallback(this::onDisplayRefreshCycleBegin);

    long lastOpenDeltaNanos = refreshTimeNanos - lastOpenedTimeNanos;
    long refreshDeltaNanos = refreshTimeNanos - lastRefreshTimeNanos;
    lastRefreshTimeNanos = refreshTimeNanos;

    // Make a greedy choice whether to open (or keep open) the render window. If the current time
    // since the render window was last opened is closer to the target than what we predict it would
    // be in the next refresh cycle then we open the window.
    if (Math.abs(lastOpenDeltaNanos - targetFrameIntervalNanos)
        < Math.abs(lastOpenDeltaNanos - targetFrameIntervalNanos + refreshDeltaNanos)) {
      lastOpenedTimeNanos = refreshTimeNanos;
      openRenderWindow();
    } else if (renderWindowOpen) {
      closeRenderWindow();
    }
  }

  private void openRenderWindow() {
    renderWindowOpen = true;
    forEachListener(Listener::onRenderWindowOpen);
  }

  private void closeRenderWindow() {
    renderWindowOpen = false;
    forEachListener(Listener::onRenderWindowClose);
  }

  private void forEachListener(Consumer<Listener> consumer) {
    Listener[] listenersCopy;
    synchronized (lock) {
      // Iterate over a copy in case the callback modifies the listener list.
      listenersCopy = listeners.toArray(new Listener[0]);
    }
    for (Listener listener : listenersCopy) {
      consumer.accept(listener);
    }
  }
}
