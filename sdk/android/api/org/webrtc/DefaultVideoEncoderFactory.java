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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class DefaultVideoEncoderFactory implements VideoEncoderFactory {
  private final HardwareVideoEncoderFactory hardwareVideoEncoderFactory;
  private final SoftwareVideoEncoderFactory softwareVideoEncoderFactory;

  public DefaultVideoEncoderFactory(
      EglBase.Context eglContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    hardwareVideoEncoderFactory = new HardwareVideoEncoderFactory(
        eglContext, enableIntelVp8Encoder, enableH264HighProfile, false /* fallbackToSoftware */);
    softwareVideoEncoderFactory = new SoftwareVideoEncoderFactory();
  }

  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    VideoEncoder encoder = hardwareVideoEncoderFactory.createEncoder(info);
    if (encoder != null) {
      return encoder;
    }
    return softwareVideoEncoderFactory.createEncoder(info);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    List<VideoCodecInfo> supportedCodecInfos = new ArrayList<VideoCodecInfo>();

    supportedCodecInfos.addAll(Arrays.asList(softwareVideoEncoderFactory.getSupportedCodecs()));

    for (VideoCodecInfo info : hardwareVideoEncoderFactory.getSupportedCodecs()) {
      if (!containsSameCodec(supportedCodecInfos, info)) {
        supportedCodecInfos.add(info);
      }
    }

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }

  private static boolean containsSameCodec(List<VideoCodecInfo> infos, VideoCodecInfo info) {
    for (VideoCodecInfo otherInfo : infos) {
      if (isSameCodec(info, otherInfo)) {
        return true;
      }
    }
    return false;
  }

  private static native boolean isSameCodec(VideoCodecInfo info1, VideoCodecInfo info2);
}
