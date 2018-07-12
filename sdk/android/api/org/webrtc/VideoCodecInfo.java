/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import javax.annotation.Nullable;
import java.util.Arrays;
import java.util.Locale;
import java.util.Map;
import java.util.HashMap;

/**
 * Represent a video codec as encoded in SDP.
 */
public class VideoCodecInfo {
  // Keys for H264 VideoCodecInfo properties.
  public static final String H264_FMTP_PROFILE_LEVEL_ID = "profile-level-id";
  public static final String H264_FMTP_LEVEL_ASYMMETRY_ALLOWED = "level-asymmetry-allowed";
  public static final String H264_FMTP_PACKETIZATION_MODE = "packetization-mode";

  public static final String H264_PROFILE_CONSTRAINED_BASELINE = "42e0";
  public static final String H264_PROFILE_CONSTRAINED_HIGH = "640c";
  public static final String H264_LEVEL_3_1 = "1f"; // 31 in hex.
  public static final String H264_CONSTRAINED_HIGH_3_1 =
      H264_PROFILE_CONSTRAINED_HIGH + H264_LEVEL_3_1;
  public static final String H264_CONSTRAINED_BASELINE_3_1 =
      H264_PROFILE_CONSTRAINED_BASELINE + H264_LEVEL_3_1;

  public static Map<String, String> getH264Params(boolean isHighProfile) {
    final Map<String, String> params = new HashMap<>();
    params.put(VideoCodecInfo.H264_FMTP_LEVEL_ASYMMETRY_ALLOWED, "1");
    params.put(VideoCodecInfo.H264_FMTP_PACKETIZATION_MODE, "1");
    params.put(VideoCodecInfo.H264_FMTP_PROFILE_LEVEL_ID,
        isHighProfile ? VideoCodecInfo.H264_CONSTRAINED_HIGH_3_1
                      : VideoCodecInfo.H264_CONSTRAINED_BASELINE_3_1);
    return params;
  }

  public static VideoCodecInfo VP8_CODEC = new VideoCodecInfo("VP8", new HashMap<>());
  public static VideoCodecInfo VP9_CODEC = new VideoCodecInfo("VP9", new HashMap<>());
  public static VideoCodecInfo H264_BASELINE_PROFILE_CODEC =
      new VideoCodecInfo("H264", getH264Params(/* isHighProfile= */ false));
  public static VideoCodecInfo H264_HIGH_PROFILE_CODEC =
      new VideoCodecInfo("H264", getH264Params(/* isHighProfile= */ true));

  /**
   * Returns true if the codecs are the same general codec. In case of H264, it is necessary to
   * look at the parameters.
   */
  public static boolean isSameCodec(VideoCodecInfo codecA, VideoCodecInfo codecB) {
    if (!codecA.name.equalsIgnoreCase(codecB.name)) {
      return false;
    }
    return codecA.name.equalsIgnoreCase("H264")
        ? nativeIsSameH264Profile(codecA.params, codecB.params)
        : true;
  }

  /** Returns true if any codec in the array of supported codecs match the specified codec. */
  public static boolean isCodecSupported(VideoCodecInfo[] supportedCodecs, VideoCodecInfo codec) {
    for (VideoCodecInfo supportedCodec : supportedCodecs) {
      if (isSameCodec(supportedCodec, codec)) {
        return true;
      }
    }
    return false;
  }

  public final String name;
  public final Map<String, String> params;
  @Deprecated public final int payload;

  @CalledByNative
  public VideoCodecInfo(String name, Map<String, String> params) {
    this.payload = 0;
    this.name = name;
    this.params = params;
  }

  @Deprecated
  public VideoCodecInfo(int payload, String name, Map<String, String> params) {
    this.payload = payload;
    this.name = name;
    this.params = params;
  }

  @Override
  public boolean equals(@Nullable Object obj) {
    if (obj == null)
      return false;
    if (obj == this)
      return true;
    if (!(obj instanceof VideoCodecInfo))
      return false;

    VideoCodecInfo otherInfo = (VideoCodecInfo) obj;
    return name.equalsIgnoreCase(otherInfo.name) && params.equals(otherInfo.params);
  }

  @Override
  public int hashCode() {
    Object[] values = {name.toUpperCase(Locale.ROOT), params};
    return Arrays.hashCode(values);
  }

  @CalledByNative
  String getName() {
    return name;
  }

  @CalledByNative
  Map getParams() {
    return params;
  }

  private static native boolean nativeIsSameH264Profile(
      Map<String, String> params1, Map<String, String> params2);
}
