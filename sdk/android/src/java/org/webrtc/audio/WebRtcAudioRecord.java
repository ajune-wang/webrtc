/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioRecordingConfiguration;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.os.Process;
import android.support.annotation.Nullable;
import java.lang.System;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import org.webrtc.CalledByNative;
import org.webrtc.Logging;
import org.webrtc.ThreadUtils;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordErrorCallback;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordStartErrorCode;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordStateCallback;
import org.webrtc.audio.JavaAudioDeviceModule.SamplesReadyCallback;

class WebRtcAudioRecord {
  private static final String TAG = "WebRtcAudioRecordExternal";

  // Requested size of each recorded buffer provided to the client.
  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  // Average number of callbacks per second.
  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  // We ask for a native buffer size of BUFFER_SIZE_FACTOR * (minimum required
  // buffer size). The extra space is allocated to guard against glitches under
  // high load.
  private static final int BUFFER_SIZE_FACTOR = 2;

  // The AudioRecordJavaThread is allowed to wait for successful call to join()
  // but the wait times out afther this amount of time.
  private static final long AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS = 2000;

  public static final int DEFAULT_AUDIO_SOURCE = AudioSource.VOICE_COMMUNICATION;

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  public static final int DEFAULT_AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT;

  // Indicates AudioRecord has started recording audio.
  private static final int AUDIO_RECORD_START = 0;

  // Indicates AudioRecord has stopped recording audio.
  private static final int AUDIO_RECORD_STOP = 1;

  // Time to wait before checking recording status after start has been called. Tests have
  // shown that the result can sometimes be invalid (our own status might be missing) if we check
  // directly after start.
  private static final int CHECK_REC_STATUS_DELAY_MS = 100;

  private final Context context;
  private final AudioManager audioManager;
  private final int audioSource;
  private final int audioFormat;

  private long nativeAudioRecord;

  private final WebRtcAudioEffects effects = new WebRtcAudioEffects();

  private @Nullable ByteBuffer byteBuffer;

  private @Nullable AudioRecord audioRecord;
  private @Nullable AudioRecordThread audioThread;

  private @Nullable ScheduledExecutorService executor;
  private @Nullable ScheduledFuture<String> future;

  private volatile boolean microphoneMute;
  private byte[] emptyBytes;

  private final @Nullable AudioRecordErrorCallback errorCallback;
  private final @Nullable AudioRecordStateCallback stateCallback;
  private final @Nullable SamplesReadyCallback audioSamplesReadyCallback;
  private final boolean isAcousticEchoCancelerSupported;
  private final boolean isNoiseSuppressorSupported;

  /**
   * Audio thread which keeps calling ByteBuffer.read() waiting for audio
   * to be recorded. Feeds recorded data to the native counterpart as a
   * periodic sequence of callbacks using DataIsRecorded().
   * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
   */
  private class AudioRecordThread extends Thread {
    private volatile boolean keepAlive = true;

    public AudioRecordThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      Logging.d(TAG, "AudioRecordThread" + WebRtcAudioUtils.getThreadInfo());
      assertTrue(audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING);

      // Audio recording has started and the client is informed about it.
      doAudioRecordStateCallback(AUDIO_RECORD_START);

      long lastTime = System.nanoTime();
      while (keepAlive) {
        int bytesRead = audioRecord.read(byteBuffer, byteBuffer.capacity());
        if (bytesRead == byteBuffer.capacity()) {
          if (microphoneMute) {
            byteBuffer.clear();
            byteBuffer.put(emptyBytes);
          }
          // It's possible we've been shut down during the read, and stopRecording() tried and
          // failed to join this thread. To be a bit safer, try to avoid calling any native methods
          // in case they've been unregistered after stopRecording() returned.
          if (keepAlive) {
            nativeDataIsRecorded(nativeAudioRecord, bytesRead);
          }
          if (audioSamplesReadyCallback != null) {
            // Copy the entire byte buffer array. The start of the byteBuffer is not necessarily
            // at index 0.
            byte[] data = Arrays.copyOfRange(byteBuffer.array(), byteBuffer.arrayOffset(),
                byteBuffer.capacity() + byteBuffer.arrayOffset());
            audioSamplesReadyCallback.onWebRtcAudioRecordSamplesReady(
                new JavaAudioDeviceModule.AudioSamples(audioRecord.getAudioFormat(),
                    audioRecord.getChannelCount(), audioRecord.getSampleRate(), data));
          }
        } else {
          String errorMessage = "AudioRecord.read failed: " + bytesRead;
          Logging.e(TAG, errorMessage);
          if (bytesRead == AudioRecord.ERROR_INVALID_OPERATION) {
            keepAlive = false;
            reportWebRtcAudioRecordError(errorMessage);
          }
        }
      }

      try {
        if (audioRecord != null) {
          audioRecord.stop();
          doAudioRecordStateCallback(AUDIO_RECORD_STOP);
        }
      } catch (IllegalStateException e) {
        Logging.e(TAG, "AudioRecord.stop failed: " + e.getMessage());
      }
    }

    // Stops the inner thread loop and also calls AudioRecord.stop().
    // Does not block the calling thread.
    public void stopThread() {
      Logging.d(TAG, "stopThread");
      keepAlive = false;
    }
  }

  @CalledByNative
  WebRtcAudioRecord(Context context, AudioManager audioManager) {
    this(context, audioManager, DEFAULT_AUDIO_SOURCE, DEFAULT_AUDIO_FORMAT,
        null /* errorCallback */, null /* stateCallback */, null /* audioSamplesReadyCallback */,
        WebRtcAudioEffects.isAcousticEchoCancelerSupported(),
        WebRtcAudioEffects.isNoiseSuppressorSupported());
  }

  public WebRtcAudioRecord(Context context, AudioManager audioManager, int audioSource,
      int audioFormat, @Nullable AudioRecordErrorCallback errorCallback,
      @Nullable AudioRecordStateCallback stateCallback,
      @Nullable SamplesReadyCallback audioSamplesReadyCallback,
      boolean isAcousticEchoCancelerSupported, boolean isNoiseSuppressorSupported) {
    if (isAcousticEchoCancelerSupported && !WebRtcAudioEffects.isAcousticEchoCancelerSupported()) {
      throw new IllegalArgumentException("HW AEC not supported");
    }
    if (isNoiseSuppressorSupported && !WebRtcAudioEffects.isNoiseSuppressorSupported()) {
      throw new IllegalArgumentException("HW NS not supported");
    }
    this.context = context;
    this.audioManager = audioManager;
    this.audioSource = audioSource;
    this.audioFormat = audioFormat;
    this.errorCallback = errorCallback;
    this.stateCallback = stateCallback;
    this.audioSamplesReadyCallback = audioSamplesReadyCallback;
    this.isAcousticEchoCancelerSupported = isAcousticEchoCancelerSupported;
    this.isNoiseSuppressorSupported = isNoiseSuppressorSupported;
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
  }

  @CalledByNative
  public void setNativeAudioRecord(long nativeAudioRecord) {
    this.nativeAudioRecord = nativeAudioRecord;
  }

  @CalledByNative
  boolean isAcousticEchoCancelerSupported() {
    return isAcousticEchoCancelerSupported;
  }

  @CalledByNative
  boolean isNoiseSuppressorSupported() {
    return isNoiseSuppressorSupported;
  }

  @CalledByNative
  private boolean enableBuiltInAEC(boolean enable) {
    Logging.d(TAG, "enableBuiltInAEC(" + enable + ")");
    return effects.setAEC(enable);
  }

  @CalledByNative
  private boolean enableBuiltInNS(boolean enable) {
    Logging.d(TAG, "enableBuiltInNS(" + enable + ")");
    return effects.setNS(enable);
  }

  @CalledByNative
  private int initRecording(int sampleRate, int channels) {
    Logging.d(TAG, "initRecording(sampleRate=" + sampleRate + ", channels=" + channels + ")");
    if (audioRecord != null) {
      reportWebRtcAudioRecordInitError("InitRecording called twice without StopRecording.");
      return -1;
    }
    final int bytesPerFrame = channels * getBytesPerSample(audioFormat);
    final int framesPerBuffer = sampleRate / BUFFERS_PER_SECOND;
    byteBuffer = ByteBuffer.allocateDirect(bytesPerFrame * framesPerBuffer);
    if (!(byteBuffer.hasArray())) {
      reportWebRtcAudioRecordInitError("ByteBuffer does not have backing array.");
      return -1;
    }
    Logging.d(TAG, "byteBuffer.capacity: " + byteBuffer.capacity());
    emptyBytes = new byte[byteBuffer.capacity()];
    // Rather than passing the ByteBuffer with every callback (requiring
    // the potentially expensive GetDirectBufferAddress) we simply have the
    // the native class cache the address to the memory once.
    nativeCacheDirectBufferAddress(nativeAudioRecord, byteBuffer);

    // Get the minimum buffer size required for the successful creation of
    // an AudioRecord object, in byte units.
    // Note that this size doesn't guarantee a smooth recording under load.
    final int channelConfig = channelCountToConfiguration(channels);
    int minBufferSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
    if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
      reportWebRtcAudioRecordInitError("AudioRecord.getMinBufferSize failed: " + minBufferSize);
      return -1;
    }
    Logging.d(TAG, "AudioRecord.getMinBufferSize: " + minBufferSize);

    // Use a larger buffer size than the minimum required when creating the
    // AudioRecord instance to ensure smooth recording under load. It has been
    // verified that it does not increase the actual recording latency.
    int bufferSizeInBytes = Math.max(BUFFER_SIZE_FACTOR * minBufferSize, byteBuffer.capacity());
    Logging.d(TAG, "bufferSizeInBytes: " + bufferSizeInBytes);
    try {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        // Use the AudioRecord.Builder class on Android M (23) and above.
        // Throws IllegalArgumentException.
        audioRecord = createAudioRecordOnMOrHigher(
            audioSource, sampleRate, channelConfig, audioFormat, bufferSizeInBytes);
      } else {
        // Use the old AudioRecord constructor for API levels below 23.
        // Throws UnsupportedOperationException.
        audioRecord = createAudioRecordOnLowerThanM(
            audioSource, sampleRate, channelConfig, audioFormat, bufferSizeInBytes);
      }
    } catch (IllegalArgumentException | UnsupportedOperationException e) {
      // Report of exception message is sufficient. Example: "Cannot create AudioRecord".
      reportWebRtcAudioRecordInitError(e.getMessage());
      releaseAudioResources();
      return -1;
    }
    if (audioRecord == null || audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
      reportWebRtcAudioRecordInitError("Creation or initialization of audio recorder failed.");
      releaseAudioResources();
      return -1;
    }
    effects.enable(audioRecord.getAudioSessionId());
    logMainParameters();
    logMainParametersExtended();
    // Check number of active recording sessions. Should be zero but we have seen conflict cases
    // and adding a log for it can help us figure out details about conflicting sessions.
    final int numActiveRecordingSessions = logRecordingConfigurations();
    if (numActiveRecordingSessions != 0) {
      // Log the conflict as a warning since initialization did in fact succeed. Most likely, the
      // upcoming call to startRecording() will fail under these conditions.
      Logging.w(
          TAG, "Potential microphone conflict. Active sessions: " + numActiveRecordingSessions);
    }
    return framesPerBuffer;
  }

  @CalledByNative
  private boolean startRecording() {
    Logging.d(TAG, "startRecording");
    assertTrue(audioRecord != null);
    assertTrue(audioThread == null);
    try {
      audioRecord.startRecording();
    } catch (IllegalStateException e) {
      reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_EXCEPTION,
          "AudioRecord.startRecording failed: " + e.getMessage());
      return false;
    }
    if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
      reportWebRtcAudioRecordStartError(AudioRecordStartErrorCode.AUDIO_RECORD_START_STATE_MISMATCH,
          "AudioRecord.startRecording failed - incorrect state: "
              + audioRecord.getRecordingState());
      return false;
    }
    audioThread = new AudioRecordThread("AudioRecordJavaThread");
    audioThread.start();
    scheduleLogRecordingConfigurationsTask();
    return true;
  }

  @CalledByNative
  private boolean stopRecording() {
    Logging.d(TAG, "stopRecording");
    assertTrue(audioThread != null);
    if (future != null) {
      if (!future.isDone()) {
        // Might be needed if the client calls startRecording(), stopRecording() back-to-back.
        future.cancel(true /* mayInterruptIfRunning */);
      }
      future = null;
    }
    if (executor != null) {
      executor.shutdownNow();
      executor = null;
    }
    audioThread.stopThread();
    if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS)) {
      Logging.e(TAG, "Join of AudioRecordJavaThread timed out");
      WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    }
    audioThread = null;
    effects.release();
    releaseAudioResources();
    return true;
  }

  @TargetApi(Build.VERSION_CODES.M)
  private static AudioRecord createAudioRecordOnMOrHigher(
      int audioSource, int sampleRate, int channelConfig, int audioFormat, int bufferSizeInBytes) {
    Logging.d(TAG, "createAudioRecordOnMOrHigher");
    return new AudioRecord.Builder()
        .setAudioSource(audioSource)
        .setAudioFormat(new AudioFormat.Builder()
                            .setEncoding(audioFormat)
                            .setSampleRate(sampleRate)
                            .setChannelMask(channelConfig)
                            .build())
        .setBufferSizeInBytes(bufferSizeInBytes)
        .build();
  }

  private static AudioRecord createAudioRecordOnLowerThanM(
      int audioSource, int sampleRate, int channelConfig, int audioFormat, int bufferSizeInBytes) {
    Logging.d(TAG, "createAudioRecordOnLowerThanM");
    return new AudioRecord(audioSource, sampleRate, channelConfig, audioFormat, bufferSizeInBytes);
  }

  private void logMainParameters() {
    Logging.d(TAG,
        "AudioRecord: "
            + "session ID: " + audioRecord.getAudioSessionId() + ", "
            + "channels: " + audioRecord.getChannelCount() + ", "
            + "sample rate: " + audioRecord.getSampleRate());
  }

  @TargetApi(Build.VERSION_CODES.M)
  private void logMainParametersExtended() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      Logging.d(TAG,
          "AudioRecord: "
              // The frame count of the native AudioRecord buffer.
              + "buffer size in frames: " + audioRecord.getBufferSizeInFrames());
    }
  }

  @TargetApi(Build.VERSION_CODES.N)
  // Checks the number of active recording sessions and logs the states of all active sessions.
  // Returns number of active sessions.
  private int logRecordingConfigurations() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
      Logging.w(TAG, "AudioManager#getActiveRecordingConfigurations() requires N or higher");
      return 0;
    }
    // Get a list of the currently active audio recording configurations of the device (can be more
    // than one). An empty list indicates there is no recording active when queried.
    List<AudioRecordingConfiguration> configs = audioManager.getActiveRecordingConfigurations();
    final int numActiveRecordingSessions = configs.size();
    Logging.d(TAG, "Number of active recording sessions: " + numActiveRecordingSessions);
    if (numActiveRecordingSessions > 0) {
      logActiveRecordingConfigs(audioRecord.getAudioSessionId(), configs);
    }
    return numActiveRecordingSessions;
  }

  // Helper method which throws an exception  when an assertion has failed.
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private int channelCountToConfiguration(int channels) {
    return (channels == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
  }

  private native void nativeCacheDirectBufferAddress(
      long nativeAudioRecordJni, ByteBuffer byteBuffer);
  private native void nativeDataIsRecorded(long nativeAudioRecordJni, int bytes);

  // Sets all recorded samples to zero if |mute| is true, i.e., ensures that
  // the microphone is muted.
  public void setMicrophoneMute(boolean mute) {
    Logging.w(TAG, "setMicrophoneMute(" + mute + ")");
    microphoneMute = mute;
  }

  // Releases the native AudioRecord resources.
  private void releaseAudioResources() {
    Logging.d(TAG, "releaseAudioResources");
    if (audioRecord != null) {
      audioRecord.release();
      audioRecord = null;
    }
  }

  private void reportWebRtcAudioRecordInitError(String errorMessage) {
    Logging.e(TAG, "Init recording error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    logRecordingConfigurations();
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordInitError(errorMessage);
    }
  }

  private void reportWebRtcAudioRecordStartError(
      AudioRecordStartErrorCode errorCode, String errorMessage) {
    Logging.e(TAG, "Start recording error: " + errorCode + ". " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    logRecordingConfigurations();
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordStartError(errorCode, errorMessage);
    }
  }

  private void reportWebRtcAudioRecordError(String errorMessage) {
    Logging.e(TAG, "Run-time recording error: " + errorMessage);
    WebRtcAudioUtils.logAudioState(TAG, context, audioManager);
    if (errorCallback != null) {
      errorCallback.onWebRtcAudioRecordError(errorMessage);
    }
  }

  private void doAudioRecordStateCallback(int audioState) {
    Logging.d(TAG, "doAudioRecordStateCallback: " + audioStateToString(audioState));
    if (stateCallback != null) {
      if (audioState == WebRtcAudioRecord.AUDIO_RECORD_START) {
        stateCallback.onWebRtcAudioRecordStart();
      } else if (audioState == WebRtcAudioRecord.AUDIO_RECORD_STOP) {
        stateCallback.onWebRtcAudioRecordStop();
      } else {
        Logging.e(TAG, "Invalid audio state");
      }
    }
  }

  // Reference from Android code, AudioFormat.getBytesPerSample. BitPerSample / 8
  // Default audio data format is PCM 16 bits per sample.
  // Guaranteed to be supported by all devices
  private static int getBytesPerSample(int audioFormat) {
    switch (audioFormat) {
      case AudioFormat.ENCODING_PCM_8BIT:
        return 1;
      case AudioFormat.ENCODING_PCM_16BIT:
      case AudioFormat.ENCODING_IEC61937:
      case AudioFormat.ENCODING_DEFAULT:
        return 2;
      case AudioFormat.ENCODING_PCM_FLOAT:
        return 4;
      case AudioFormat.ENCODING_INVALID:
      default:
        throw new IllegalArgumentException("Bad audio format " + audioFormat);
    }
  }

  // Use an ExecutorService to schedule a task after a given delay where the task consists of
  // checking (by logging) the current status of active recording sessions.
  private void scheduleLogRecordingConfigurationsTask() {
    Logging.d(TAG, "scheduleLogRecordingConfigurationsTask");
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
      return;
    }
    if (executor != null) {
      executor.shutdownNow();
      executor = null;
    }
    executor = Executors.newSingleThreadScheduledExecutor();

    Callable<String> callable = () -> {
      logRecordingConfigurations();
      return "Scheduled task is done";
    };

    if (future != null && !future.isDone()) {
      future.cancel(true /* mayInterruptIfRunning */);
      future = null;
    }
    // Schedule call to logRecordingConfigurations() from executor thread after fixed delay.
    future = executor.schedule(callable, CHECK_REC_STATUS_DELAY_MS, TimeUnit.MILLISECONDS);
  };

  @TargetApi(Build.VERSION_CODES.N)
  private static boolean logActiveRecordingConfigs(
      int session, List<AudioRecordingConfiguration> configs) {
    assertTrue(!configs.isEmpty());
    Iterator<AudioRecordingConfiguration> it = configs.iterator();
    Logging.d(TAG, "AudioRecordingConfigurations: ");
    while (it.hasNext()) {
      final AudioRecordingConfiguration config = it.next();
      StringBuilder conf = new StringBuilder();
      // The audio source selected by the client.
      final int audioSource = config.getClientAudioSource();
      conf.append("  client audio source=")
          .append(audioSourceToString(audioSource))
          .append(", client session id=")
          .append(config.getClientAudioSessionId())
          // Compare with our own id (based on AudioRecord#getAudioSessionId()).
          .append(" (")
          .append(session)
          .append(")")
          .append("\n");
      // Audio format at which audio is recorded on this Android device. Note that it may differ
      // from the client application recording format (see getClientFormat()).
      AudioFormat format = config.getFormat();
      conf.append("  Device AudioFormat: ")
          .append("channel count=")
          .append(format.getChannelCount())
          .append(", channel index mask=")
          .append(format.getChannelIndexMask())
          .append(", channel index=")
          .append(format.getChannelMask())
          .append(", encoding=")
          .append(format.getEncoding())
          .append(", sample rate=")
          .append(format.getSampleRate())
          .append("\n");
      // Audio format at which the client application is recording audio.
      format = config.getClientFormat();
      conf.append("  Client AudioFormat: ")
          .append("channel count=")
          .append(format.getChannelCount())
          .append(", channel index mask=")
          .append(format.getChannelIndexMask())
          .append(", channel index=")
          .append(format.getChannelMask())
          .append(", encoding=")
          .append(format.getEncoding())
          .append(", sample rate=")
          .append(format.getSampleRate())
          .append("\n");
      // Audio input device used for this recording session.
      final AudioDeviceInfo device = config.getAudioDevice();
      assertTrue(device.isSource());
      conf.append("  AudioDevice: ")
          .append("type=")
          .append(WebRtcAudioUtils.deviceTypeToString(device.getType()))
          .append(", id=")
          .append(device.getId());
      Logging.d(TAG, conf.toString());
    }
    return true;
  }

  @TargetApi(Build.VERSION_CODES.N)
  private static String audioSourceToString(int source) {
    // AudioSource.UNPROCESSED requires API level 29. Use local define instead.
    final int VOICE_PERFORMANCE = 10;
    switch (source) {
      case AudioSource.DEFAULT:
        return "DEFAULT";
      case AudioSource.MIC:
        return "MIC";
      case AudioSource.VOICE_UPLINK:
        return "VOICE_UPLINK";
      case AudioSource.VOICE_DOWNLINK:
        return "VOICE_DOWNLINK";
      case AudioSource.VOICE_CALL:
        return "VOICE_CALL";
      case AudioSource.CAMCORDER:
        return "CAMCORDER";
      case AudioSource.VOICE_RECOGNITION:
        return "VOICE_RECOGNITION";
      case AudioSource.VOICE_COMMUNICATION:
        return "VOICE_COMMUNICATION";
      case AudioSource.UNPROCESSED:
        return "UNPROCESSED";
      case VOICE_PERFORMANCE:
        return "VOICE_PERFORMANCE";
      default:
        return "INVALID";
    }
  }

  private static String audioStateToString(int state) {
    switch (state) {
      case WebRtcAudioRecord.AUDIO_RECORD_START:
        return "START";
      case WebRtcAudioRecord.AUDIO_RECORD_STOP:
        return "STOP";
      default:
        return "INVALID";
    }
  }

  /*

  // getRoutedDevice requires Android 23 (M)

  // verify our recording shows as one of the recording configs
        assertTrue("Test source/session not amongst active record configurations",
                verifyAudioConfig(TEST_AUDIO_SOURCE, mAudioRecord.getAudioSessionId(),
                        mAudioRecord.getFormat(), mAudioRecord.getRoutedDevice(), configs));


  final AudioDeviceInfo testDevice = mAudioRecord.getRoutedDevice();
            assertTrue("AudioRecord null routed device after start", testDevice != null);
            final boolean match = verifyAudioConfig(mAudioRecord.getAudioSource(),
                    mAudioRecord.getAudioSessionId(), mAudioRecord.getFormat(),
                    testDevice, callback.mConfigs);


   private static boolean deviceMatch(AudioDeviceInfo devJoe, AudioDeviceInfo devJeff) {
        return ((devJoe.getId() == devJeff.getId()
                && (devJoe.getAddress() == devJeff.getAddress())
                && (devJoe.getType() == devJeff.getType())));
    }

    private static boolean verifyAudioConfig(int source, int session, AudioFormat format,
            AudioDeviceInfo device, List<AudioRecordingConfiguration> configs) {
        final Iterator<AudioRecordingConfiguration> confIt = configs.iterator();
        while (confIt.hasNext()) {
            final AudioRecordingConfiguration config = confIt.next();
            final AudioDeviceInfo configDevice = config.getAudioDevice();
            assertTrue("Current recording config has null device", configDevice != null);
            if ((config.getClientAudioSource() == source)
                    && (config.getClientAudioSessionId() == session)
                    // test the client format matches that requested (same as the AudioRecord's)
                    && (config.getClientFormat().getEncoding() == format.getEncoding())
                    && (config.getClientFormat().getSampleRate() == format.getSampleRate())
                    && (config.getClientFormat().getChannelMask() == format.getChannelMask())
                    && (config.getClientFormat().getChannelIndexMask() ==
                            format.getChannelIndexMask())
                    // test the device format is configured
                    && (config.getFormat().getEncoding() != AudioFormat.ENCODING_INVALID)
                    && (config.getFormat().getSampleRate() > 0)
                    //  for the channel mask, either the position or index-based value must be valid
                    && ((config.getFormat().getChannelMask() != AudioFormat.CHANNEL_INVALID)
                            || (config.getFormat().getChannelIndexMask() !=
                                    AudioFormat.CHANNEL_INVALID))
                    && deviceMatch(device, configDevice)) {
                return true;
            }
        }
        return false;
    }
  */

  /*
    private void logAudioRecordingConfiguration(List<AudioRecordingConfiguration>
     configs) {
      if (!configs.isEmpty()) {
        Iterator<AudioRecordingConfiguration> it = configs.iterator();
        while (it.hasNext()) {
          final AudioRecordingConfiguration config = it.next();
          AudioDeviceInfo info = config.getAudioDevice();
         String typeString = "UNDEFINED";
          if (info != null) {
            // Seems like AudioDeviceInfo.getAudioDevice() can return null sometim.
            typeString = audioDeviceTypeToString(info.getType());
          }
          Log.d(TAG, "AudioRecordingConfiguration: "
                  + "audio source=" + audioSourceToString(config.getClientAudioSou
     rce())
                  + ", device type=" + typeString);
        }
      }
    }

    */
}
