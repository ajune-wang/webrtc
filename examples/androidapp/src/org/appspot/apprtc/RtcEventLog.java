/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import java.io.File;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import org.webrtc.PeerConnection;

public class RtcEventLog {
  private static final String TAG = "RtcEventLog";
  public static final String LOG_OUTPUT_DIR_NAME = "rtc_event_log";
  public static final int UPLOAD_FILE_MAX_BYTES = 10_000_000;
  public final String LOG_OUTPUT_FILE_NAME;
  private PeerConnection peerConnection;
  private RtcEventLogState state;

  enum RtcEventLogState {
    INACTIVE,
    STARTED,
    STOPPED,
  }

  public RtcEventLog(PeerConnection peerConnection) {
    this.peerConnection = peerConnection;
    state = RtcEventLogState.INACTIVE;
    DateFormat dateFormat = new SimpleDateFormat("yyyyMMdd_hhmm_ss", Locale.getDefault());
    Date date = new Date();
    LOG_OUTPUT_FILE_NAME = "event_log_" + dateFormat.format(date) + ".log";
  }

  public File createLogOutputFile(Context context) {
    return new File(
        context.getDir(LOG_OUTPUT_DIR_NAME, Context.MODE_PRIVATE), LOG_OUTPUT_FILE_NAME);
  }

  public void start(final Context appContext) {
    File outputFile = createLogOutputFile(appContext);
    ParcelFileDescriptor fileDescriptor = null;
    try {
      fileDescriptor = ParcelFileDescriptor.open(outputFile,
          ParcelFileDescriptor.MODE_READ_WRITE | ParcelFileDescriptor.MODE_CREATE
              | ParcelFileDescriptor.MODE_TRUNCATE);
    } catch (IOException e) {
      Log.e(TAG, "Failed to create a new file", e);
      return;
    }

    // Passes ownership of the file to WebRTC.
    boolean success =
        peerConnection.startRtcEventLog(fileDescriptor.detachFd(), UPLOAD_FILE_MAX_BYTES);
    try {
      fileDescriptor.close();
    } catch (IOException e) {
      Log.e(TAG, "Failed to close the file descriptor", e);
    }
    if (!success) {
      Log.e(TAG, "Failed to start RTC event log.");
      return;
    }
    state = RtcEventLogState.STARTED;
    Log.d(TAG, "RtcEventLog started.");
  }

  public void stop() {
    if (state != RtcEventLogState.STARTED) {
      Log.e(TAG, "RTC event log was not started.");
      return;
    }
    peerConnection.stopRtcEventLog();
    state = RtcEventLogState.STOPPED;
    Log.d(TAG, "RtcEventLog stopped.");
  }
}