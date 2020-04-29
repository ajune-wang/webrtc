#!/usr/bin/env python3
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script to rename the WebRTC OBJC API symbols.
"""

# The main idea is to search for files where the WebRTC OBJC API is declared,
# defined and used, here is a list of what does it mean:
#
# - Every .m and .mm file in the WebRTC tree has a good chance to be a client
#   of the WebRTC OBJC API or to contain definitions of such symbols.
# - Every .h file under sdk/objc has a good chance to contain a declaration of
#   the symbols that need to be renamed.
# - Every .h file under examples/objc has a good chance to contain a reference
#   to the symbols that need to be renamed.
#
# The WebRTC OBJC API symbols might have identical symbols in the "rtc" C++
# namespace and so this needs to be kept into account, for example the script
# must wrap RTCIceCandidate in the RTC_OBJC_TYPE macro but not references to
# rtc::RTCIceCandidate. For this reason, the regex used for this replacement
# work is:
#
#         r'(?<!::)\b%s\b(?!.*\.h)'
#
# Which search for the symbol, not prefixed with :: and not suffixed with ".h"
# since it doesn't have to rename the #include/#import in case the filename
# has the same name of the type to rename.

import os
import re

# TODO(mbonadei/kwiberg): What should we do with these constants?
# They have the potential to create the same harm since they are part of the
# API and if they have different values we might be in trouble the same way as
# with functions/types.
#
# RTC_OBJC_EXPORT extern NSString* const kRTCVideoCodecVp8Name;
# RTC_OBJC_EXPORT extern NSString* const kRTCVideoCodecVp9Name;
# RTC_OBJC_EXPORT extern NSString *const kRTCVideoCodecH264Name;
# RTC_OBJC_EXPORT extern NSString *const kRTCLevel31ConstrainedHigh;
# RTC_OBJC_EXPORT extern NSString *const kRTCLevel31ConstrainedBaseline;
# RTC_OBJC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedHigh;
# RTC_OBJC_EXPORT extern NSString *const kRTCMaxSupportedH264ProfileLevelConstrainedBaseline;

# List manually compiled by mbonadei@ by searching for all the RTC_OBJC_EXPORT
# types in the WebRTC tree.
TO_REPLACE = [
    'RTCAudioSession',
    'RTCAudioSessionActivationDelegate',
    'RTCAudioSessionConfiguration',
    'RTCAudioSessionDelegate',
    'RTCAudioSource',
    'RTCAudioTrack',
    'RTCCVPixelBuffer',
    'RTCCallbackLogger',
    'RTCCameraPreviewView',
    'RTCCameraVideoCapturer',
    'RTCCertificate',
    'RTCCodecSpecificInfo',
    'RTCCodecSpecificInfoH264',
    'RTCConfiguration',
    'RTCCryptoOptions',
    'RTCDataBuffer',
    'RTCDataChannel',
    'RTCDataChannelConfiguration',
    'RTCDataChannelDelegate',
    'RTCDefaultVideoDecoderFactory',
    'RTCDefaultVideoEncoderFactory',
    'RTCDispatcher',
    'RTCDtmfSender',
    'RTCEAGLVideoView',
    'RTCEncodedImage',
    'RTCFileLogger',
    'RTCFileVideoCapturer',
    'RTCH264ProfileLevelId',
    'RTCI420Buffer',
    'RTCIceCandidate',
    'RTCIceServer',
    'RTCLegacyStatsReport',
    'RTCMTLNSVideoView',
    'RTCMTLVideoView',
    'RTCMediaConstraints',
    'RTCMediaSource',
    'RTCMediaStream',
    'RTCMediaStreamTrack',
    'RTCMetricsSampleInfo',
    'RTCMutableI420Buffer',
    'RTCMutableYUVPlanarBuffer',
    'RTCNSGLVideoView',
    'RTCNSGLVideoViewDelegate',
    'RTCPeerConnection',
    'RTCPeerConnectionDelegate',
    'RTCPeerConnectionFactory',
    'RTCPeerConnectionFactoryOptions',
    'RTCRtcpParameters',
    'RTCRtpCodecParameters',
    'RTCRtpEncodingParameters',
    'RTCRtpFragmentationHeader',
    'RTCRtpHeaderExtension',
    'RTCRtpParameters',
    'RTCRtpReceiver',
    'RTCRtpReceiverDelegate',
    'RTCRtpSender',
    'RTCRtpTransceiver',
    'RTCRtpTransceiverInit',
    'RTCSessionDescription',
    'RTCVideoCapturer',
    'RTCVideoCapturerDelegate',
    'RTCVideoCodecInfo',
    'RTCVideoDecoder',
    'RTCVideoDecoderFactory',
    'RTCVideoDecoderFactoryH264',
    'RTCVideoDecoderH264',
    'RTCVideoDecoderVP8',
    'RTCVideoDecoderVP9',
    'RTCVideoEncoder',
    'RTCVideoEncoderFactory',
    'RTCVideoEncoderFactoryH264',
    'RTCVideoEncoderH264',
    'RTCVideoEncoderQpThresholds',
    'RTCVideoEncoderSelector',
    'RTCVideoEncoderSettings',
    'RTCVideoEncoderVP8',
    'RTCVideoEncoderVP9',
    'RTCVideoFrame',
    'RTCVideoFrameBuffer',
    'RTCVideoRenderer',
    'RTCVideoSource',
    'RTCVideoTrack',
    'RTCVideoViewDelegate',
    'RTCVideoViewShading',
    'RTCYUVPlanarBuffer',
]

TOP_FOLDERS = [
    'api',
    'audio',
    'call',
    'common_audio',
    'common_video',
    'examples',
    'logging',
    'media',
    'modules',
    'p2p',
    'pc',
    'rtc_base',
    'sdk',
    'stats',
    'system_wrappers',
    'test',
    'video',
]

if __name__ == '__main__':
  for top_level_dir in TOP_FOLDERS:
    for subdir, dirs, files in os.walk('src/' + top_level_dir):
      for file_name in files:
        file_path = os.path.join(subdir, file_name)
        if not (file_path.endswith(('.m', '.mm')) or
                (file_path.endswith('.h') and 'sdk/objc' in file_path) or
                (file_path.endswith('.h') and 'examples/objc' in file_path)):
          print('Skipping {}'.format(file_path))
          continue

        for symbol in TO_REPLACE:
          with open(file_path) as f:
            text = f.read().splitlines()

          reg = re.compile(r'(?<!::)\b%s\b(?!.*\.h)' % symbol)

          with open(file_path, 'w') as f:
            for line in text:
              f.write(re.sub(reg, 'RTC_OBJC_TYPE(' + symbol + ')', line) + '\n')

