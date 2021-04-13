<?% config.freshness.reviewed = '2021-04-13' %?>

<?% config.freshness.owner = 'eshr' %?>

# Video Adaptation

Video adaptation is a WebRTC which reduces encoded video quality in order to
reduce CPU or bandwidth usage.

## Overview

Adaptation occurs when a _Resource_ signals that it is currently underused or
overused. When overused, the video quality is decreased and when underused, the
video quality is increased. The algorithm for changing the video quality is
based on the degredation preference for the video track.

## Resources

_Resources_ monitor metrics from the system or the video stream. For example, a
resource could monitor system temperature or the bandwidth usage of the video
stream. A resource implements the [Resource][resource.h] interface. When a
resource detects that it is overused, it calls `SetUsageState(kOveruse)`. When
the resource is no longer overused, it can signal this using
`SetUsageState(kUnderuse)`.

There are two resources that are used by default on all video tracks: Quality
scaler resource and encode overuse resource.

### QP Scaler Resource

The quality scaler resource monitors the quantization parameter (QP) of the
encoded video frames for video send stream and ensures that the quality of the
stream is acceptable for the current resolution. After each frame is encoded the
[QualityScaler][quality_scaler.h] is given the QP of the encoded frame. Overuse
or underuse is signalled when the average QP is outside of the
[QP thresholds][VideoEncoder::QpThresholds]. If the average QP is above the
_high_ threshold, the QP scaler signals _overuse_, and when below the _low_
threshold the QP scaler signals _underuse_.

The thresholds are set by the video encoder in the `scaling_settings` property
of the [EncoderInfo][EncoderInfo].

*Note:* that the QP scaler is only enabled when the degradation preference is
`MAINTAIN_FRAMERATE` or `BALANCED`.

### Encode Usage Resouce

The [encoder usage resource][encode_usage_resource.h] montiors how long it takes
to encode a video frame. This works as a good proxy measurement for CPU usage as
contention increases when CPU usage is high, increasing the encode times of the
video frames.

The time is tracked from when frame encoding starts to when it is completed. If
the average encoder usage exceeds the thresholds set, *overuse* is triggered.

These thresholds can be configured but by default are 42%-85% low and high
thresholds for software encoders, and 150%-200% low and high thresholds for
hardware encoders.

### Injecting other Resources

A custom resource can be injected into the call using the
[Call::AddAdaptationResource][Call::AddAdaptationResource] method.

## Degradation

The video degradation logic is performed by the
[VideoStreamAdapter][VideoStreamAdapter].

### Degradation Preference

There are 4 degredation preferences, described in the
[rtp_parameters.h][RtpParameters] header. These are

*   `MAINTIAIN_FRAMERATE`: Adapt video resolution
*   `MAINTIAIN_RESOLUTION`: Adapt video framerate.
*   `BALANCED`: Adapt video framerate or resolution.
*   `DISABLED`: Disabled adaptation.

The degradation preference is set for a video track using the
`degradation_preference` property in the [RtpParameters][RtpParamters].

[RtpParamters]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/rtp_parameters.h?q=%22RTC_EXPORT%20RtpParameters%22
[VideoStreamAdapted]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/call/adaptation/video_stream_adapter.cc
[resource.h]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/adaptation/resource.h
[Call::AddAdaptationResource]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/call/call.h?q=Call::AddAdaptationResource
[quality_scaler.h]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/video_coding/utility/quality_scaler.h
[VideoEncoder::QpThresholds]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/video_codecs/video_encoder.h?q=VideoEncoder::QpThresholds
[EncoderInfo]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/video_codecs/video_encoder.h?q=VideoEncoder::EncoderInfo
[encode_usage_resource.h]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/video/adaptation/encode_usage_resource.h
