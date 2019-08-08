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

import static org.webrtc.MediaCodecUtils.EXYNOS_PREFIX;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecList;
import android.os.Build;
import android.support.annotation.Nullable;
import java.util.ArrayList;
import java.util.List;

class MediaCodecVideoEncoderFactory implements VideoEncoderFactory {
  protected static final String TAG = "MediaCodecVideoEncoderFactory";

  @Nullable protected final EglBase14.Context sharedContext;
  protected final boolean enableH264HighProfile;
  @Nullable protected final Predicate<MediaCodecInfo> codecAllowedPredicate;

  public MediaCodecVideoEncoderFactory(@Nullable EglBase14.Context sharedContext,
      boolean enableH264HighProfile, @Nullable Predicate<MediaCodecInfo> codecAllowedPredicate) {
    this.sharedContext = sharedContext;
    this.enableH264HighProfile = enableH264HighProfile;
    this.codecAllowedPredicate = codecAllowedPredicate;
  }

  @Nullable
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo input) {
    // HW encoding is not supported below Android Kitkat.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null;
    }

    VideoCodecType type = VideoCodecType.valueOf(input.name);
    MediaCodecInfo info = findCodecForType(type);

    if (info == null) {
      return null;
    }

    String codecName = info.getName();
    String mime = type.mimeType();
    Integer surfaceColorFormat = MediaCodecUtils.selectColorFormat(
        MediaCodecUtils.TEXTURE_COLOR_FORMATS, info.getCapabilitiesForType(mime));
    Integer yuvColorFormat = MediaCodecUtils.selectColorFormat(
        MediaCodecUtils.ENCODER_COLOR_FORMATS, info.getCapabilitiesForType(mime));

    if (type == VideoCodecType.H264) {
      boolean isHighProfile = H264Utils.isSameH264Profile(
          input.params, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ true));
      boolean isBaselineProfile = H264Utils.isSameH264Profile(
          input.params, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ false));

      if (!isHighProfile && !isBaselineProfile) {
        return null;
      }
      if (isHighProfile && !isH264HighProfileSupported(info)) {
        return null;
      }
    }

    return new AndroidVideoEncoder(new MediaCodecWrapperFactoryImpl(), codecName, type,
        surfaceColorFormat, yuvColorFormat, input.params, getKeyFrameIntervalSec(type),
        createBitrateAdjuster(type, codecName), sharedContext);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    // HW encoding is not supported below Android Kitkat.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return new VideoCodecInfo[0];
    }

    List<VideoCodecInfo> supportedCodecInfos = new ArrayList<VideoCodecInfo>();
    // Generate a list of supported codecs in order of preference:
    // VP8, VP9, H264 (high profile), and H264 (baseline profile).
    for (VideoCodecType type :
        new VideoCodecType[] {VideoCodecType.VP8, VideoCodecType.VP9, VideoCodecType.H264}) {
      MediaCodecInfo codec = findCodecForType(type);
      if (codec != null) {
        String name = type.name();
        // TODO(sakal): Always add H264 HP once WebRTC correctly removes codecs that are not
        // supported by the decoder.
        if (type == VideoCodecType.H264 && isH264HighProfileSupported(codec)) {
          supportedCodecInfos.add(new VideoCodecInfo(
              name, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ true)));
        }

        supportedCodecInfos.add(new VideoCodecInfo(
            name, MediaCodecUtils.getCodecProperties(type, /* highProfile= */ false)));
      }
    }

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }

  private @Nullable MediaCodecInfo findCodecForType(VideoCodecType type) {
    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder codec info", e);
      }

      if (info == null || !info.isEncoder()) {
        continue;
      }

      if (isSupportedCodec(info, type)) {
        return info;
      }
    }
    return null; // No support for this type.
  }

  // Returns true if the given MediaCodecInfo indicates a supported encoder for the given type.
  private boolean isSupportedCodec(MediaCodecInfo info, VideoCodecType type) {
    if (!MediaCodecUtils.codecSupportsType(info, type)) {
      return false;
    }
    // Check for a supported color format.
    if (MediaCodecUtils.selectColorFormat(
            MediaCodecUtils.ENCODER_COLOR_FORMATS, info.getCapabilitiesForType(type.mimeType()))
        == null) {
      return false;
    }
    return isMediaCodecAllowed(info);
  }

  private boolean isMediaCodecAllowed(MediaCodecInfo info) {
    if (codecAllowedPredicate == null) {
      return true;
    }
    return codecAllowedPredicate.test(info);
  }

  private int getKeyFrameIntervalSec(VideoCodecType type) {
    switch (type) {
      case VP8: // Fallthrough intended.
      case VP9:
        return 100;
      case H264:
        return 20;
    }
    throw new IllegalArgumentException("Unsupported VideoCodecType " + type);
  }

  private BitrateAdjuster createBitrateAdjuster(VideoCodecType type, String codecName) {
    if (codecName.startsWith(EXYNOS_PREFIX)) {
      if (type == VideoCodecType.VP8) {
        // Exynos VP8 encoders need dynamic bitrate adjustment.
        return new DynamicBitrateAdjuster();
      } else {
        // Exynos VP9 and H264 encoders need framerate-based bitrate adjustment.
        return new FramerateBitrateAdjuster();
      }
    }
    // Other codecs don't need bitrate adjustment.
    return new BaseBitrateAdjuster();
  }

  private boolean isH264HighProfileSupported(MediaCodecInfo info) {
    if (!enableH264HighProfile) {
      return false;
    }

    CodecCapabilities capabilities = info.getCapabilitiesForType(VideoCodecType.H264.mimeType());
    for (CodecProfileLevel level : capabilities.profileLevels) {
      if (level.profile == CodecProfileLevel.AVCProfileHigh
          && level.level >= CodecProfileLevel.AVCLevel3) {
        return true;
      }
    }
    return false;
  }
}
