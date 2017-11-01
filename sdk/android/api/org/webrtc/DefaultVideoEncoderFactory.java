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
import java.util.List;

public class DefaultVideoEncoderFactory implements VideoEncoderFactory {
  private final HardwareVideoEncoderFactory hardwareVideoEncoderFactory;
  private final SoftwareVideoEncoderFactory softwareVideoEncoderFactory;

  public DefaultVideoEncoderFactory(
      EglBase.Context eglContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this.hardwareVideoEncoderFactory = new HardwareVideoEncoderFactory(
        eglContext, enableIntelVp8Encoder, enableH264HighProfile, false);
    this.softwareVideoEncoderFactory = new SoftwareVideoEncoderFactory();
  }

  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    VideoEncoder encoder = this.hardwareVideoEncoderFactory.createEncoder(info);
    if (encoder != null) {
      return encoder;
    }
    return this.softwareVideoEncoderFactory.createEncoder(info);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    List<VideoCodecInfo> supportedCodecInfos = new ArrayList<VideoCodecInfo>();

    for (VideoCodecInfo info : this.hardwareVideoEncoderFactory.getSupportedCodecs()) {
      supportedCodecInfos.add(info);
    }

    for (VideoCodecInfo info : this.softwareVideoEncoderFactory.getSupportedCodecs()) {
      if (!supportedCodecInfos.contains(info)) {
        supportedCodecInfos.add(info);
      }
    }

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }
}
