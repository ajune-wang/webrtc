# Audio Device Module (ADM)

<?% config.freshness.owner = 'henrika' %?> <?% config.freshness.reviewed =
'2021-04-12' %?>

## Overview

The ADM is mainly responsible for driving input (microphone) and output
(speaker) audio in WebRTC and the API is define in
[audio_device.h](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_device/include/audio_device.h;drc=eb8c4ca608486add9800f6bfb7a8ba3cf23e738es).

ADM implementations reside at two different locations in the WebRTC repository:
`modules/audio_device` and `sdk/`. The latest implementations for
[iOS](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/sdk/objc/native/api/audio_device_module.h;drc=76443eafa9375374d9f1d23da2b913f2acac6ac2)
and
[Android](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/sdk/android/src/jni/audio_device/audio_device_module.h;drc=bbeb10925eb106eeed6143ccf571bc438ec22ce1)
can be found under `sdk/`. `modules/audio_device` contains older versions for
mobile platforms and also implementations for desktop platforms such as
[Linux](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_device/linux/;drc=d15a575ec3528c252419149d35977e55269d8a41),
[Windows](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_device/win/;drc=d15a575ec3528c252419149d35977e55269d8a41)
and
[Mac OSX](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_device/mac/;drc=3b68aa346a5d3483c3448852d19d91723846825c).
This document is focusing on the parts in `modules/audio_device` and
implementation specific details such as threading models are omitted.

By default, the ADM is created in
[WebRtcVoiceEngine::Init](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/media/engine/webrtc_voice_engine.cc;l=314;drc=f7b1b95f11c74cb5369fdd528b73c70a50f2e206)
but an external implementation can also be injected using
[rtc::CreatePeerConnectionFactory](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/create_peerconnection_factory.h;l=45;drc=09ceed2165137c4bea4e02e8d3db31970d0bf273).
An example of where an external ADM is injected can be found in
[PeerConnectionInterfaceTest](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/pc/peer_connection_interface_unittest.cc;l=692;drc=2efb8a5ec61b1b87475d046c03d20244f53b14b6)
where a so-called
[fake ADM](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/pc/test/fake_audio_capture_module.h;l=42;drc=d15a575ec3528c252419149d35977e55269d8a41)
is utilized to avoid hardware dependency in a gtest. Clients can also inject
their own ADMs in situations where functionality is needed that is not provided
by the default implementations.

Unit tests of the ADM interface for desktop platforms are located
[here](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_device/audio_device_unittest.cc;l=1;drc=d15a575ec3528c252419149d35977e55269d8a41)
and they provide an overview of the basic functionality of the ADM.

## Background

The ADM interface is old and has undergone many changes over the years. It used
to be much more granular but it still contains more than 50 methods and is
implemented on several different hardware platforms.

Some APIs are not implemented on all platforms, and functionality can be spread
out differently between the methods.

The most up-to-date implementations of the ADM interface are for
[iOS](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/sdk/objc/native/api/audio_device_module.h;drc=76443eafa9375374d9f1d23da2b913f2acac6ac2)
and for
[Android](https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/sdk/android/src/jni/audio_device/audio_device_module.h;drc=bbeb10925eb106eeed6143ccf571bc438ec22ce1).
