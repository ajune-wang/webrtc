/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;
import java.nio.ByteBuffer;
import org.webrtc.NativeMethods;

/** Java wrapper for a C++ DataChannelInterface. */
public class DataChannel {
  /** Java wrapper for WebIDL RTCDataChannel. */
  public static class Init {
    public boolean ordered = true;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int maxRetransmitTimeMs = -1;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int maxRetransmits = -1;
    public String protocol = "";
    public boolean negotiated;
    // Optional unsigned short in WebIDL, -1 means unspecified.
    public int id = -1;

    @CalledByNative("Init")
    boolean getOrdered() {
      return ordered;
    }

    @CalledByNative("Init")
    int getMaxRetransmitTimeMs() {
      return maxRetransmitTimeMs;
    }

    @CalledByNative("Init")
    int getMaxRetransmits() {
      return maxRetransmits;
    }

    @CalledByNative("Init")
    String getProtocol() {
      return protocol;
    }

    @CalledByNative("Init")
    boolean getNegotiated() {
      return negotiated;
    }

    @CalledByNative("Init")
    int getId() {
      return id;
    }
  }

  /** Java version of C++ DataBuffer.  The atom of data in a DataChannel. */
  public static class Buffer {
    /** The underlying data. */
    public final ByteBuffer data;

    /**
     * Indicates whether `data` contains UTF-8 text or "binary data"
     * (i.e. anything else).
     */
    public final boolean binary;

    @CalledByNative("Buffer")
    public Buffer(ByteBuffer data, boolean binary) {
      this.data = data;
      this.binary = binary;
    }
  }

  /** Java version of C++ DataChannelObserver. */
  public interface Observer {
    /** The data channel's bufferedAmount has changed. */
    @CalledByNative("Observer") public void onBufferedAmountChange(long previousAmount);
    /** The data channel state has changed. */
    @CalledByNative("Observer") public void onStateChange();
    /**
     * A data buffer was successfully received.  NOTE: `buffer.data` will be
     * freed once this function returns so callers who want to use the data
     * asynchronously must make sure to copy it first.
     */
    @CalledByNative("Observer") public void onMessage(Buffer buffer);
  }

  /** Keep in sync with DataChannelInterface::DataState. */
  public enum State {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED;

    @CalledByNative("State")
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  private long nativeDataChannel;
  private long nativeObserver;

  @CalledByNative
  public DataChannel(long nativeDataChannel) {
    this.nativeDataChannel = nativeDataChannel;
  }

  /** Register `observer`, replacing any previously-registered observer. */
  public void registerObserver(Observer observer) {
    checkDataChannelExists();
    if (nativeObserver != 0) {
      DataChannelJni.get().unregisterObserver(this, nativeObserver);
    }
    nativeObserver = DataChannelJni.get().registerObserver(this, observer);
  }

  /** Unregister the (only) observer. */
  public void unregisterObserver() {
    checkDataChannelExists();
    DataChannelJni.get().unregisterObserver(this, nativeObserver);
    nativeObserver = 0;
  }

  public String label() {
    checkDataChannelExists();
    return DataChannelJni.get().label(this);
  }

  public int id() {
    checkDataChannelExists();
    return DataChannelJni.get().id(this);
  }

  public State state() {
    checkDataChannelExists();
    return DataChannelJni.get().state(this);
  }

  /**
   * Return the number of bytes of application data (UTF-8 text and binary data)
   * that have been queued using SendBuffer but have not yet been transmitted
   * to the network.
   */
  public long bufferedAmount() {
    checkDataChannelExists();
    return DataChannelJni.get().bufferedAmount(this);
  }

  /** Close the channel. */
  public void close() {
    checkDataChannelExists();
    DataChannelJni.get().close(this);
  }

  /** Send `data` to the remote peer; return success. */
  public boolean send(Buffer buffer) {
    checkDataChannelExists();
    // TODO(fischman): this could be cleverer about avoiding copies if the
    // ByteBuffer is direct and/or is backed by an array.
    byte[] data = new byte[buffer.data.remaining()];
    buffer.data.get(data);
    return DataChannelJni.get().send(this, data, buffer.binary);
  }

  /** Dispose of native resources attached to this channel. */
  public void dispose() {
    checkDataChannelExists();
    JniCommon.nativeReleaseRef(nativeDataChannel);
    nativeDataChannel = 0;
  }

  @CalledByNative
  long getNativeDataChannel() {
    return nativeDataChannel;
  }

  private void checkDataChannelExists() {
    if (nativeDataChannel == 0) {
      throw new IllegalStateException("DataChannel has been disposed.");
    }
  }

  @NativeMethods
  interface Natives {
    long registerObserver(DataChannel self, Observer observer);
    void unregisterObserver(DataChannel self, long observer);
    String label(DataChannel self);
    int id(DataChannel self);
    State state(DataChannel self);
    long bufferedAmount(DataChannel self);
    void close(DataChannel self);
    boolean send(DataChannel self, byte[] data, boolean binary);
  }
};
