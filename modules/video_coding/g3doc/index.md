<?% config.freshness.owner = 'brandtr' %?>
<?% config.freshness.reviewed = '2021-04-13' %?>
# Video coding in WebRTC

## Introduction to layered video coding
[Video coding][video-coding-wiki] is the process of encoding a stream of raw video frames into a compressed bitstream, whose bitrate is lower than that of the original stream.

### Block-based hybrid video coding
All video codecs in WebRTC are based on the block-based hybrid video coding paradigm, which entails [motion compensation][motion-compensation-wiki], [transform coding][transform-coding-wiki] and lossless [entropy coding][entropy-coding-wiki].

### Frame types
When an encoded frame depends on previously encoded frames (i.e., it has one or more inter-frame dependencies), the prior frames must be available at the receiver before the current frame can be decoded. In order for a receiver to start decoding an encoded stream, a frame which has no prior dependencies is thus needed. Such a frame is called a "key frame" (or "I-frame" using [MPEG][mpeg-wiki] terminology). Key frames typically compress less efficiently than "delta frames" (or "P-frames").

### Single-layer coding
In 1:1 calls, the encoded stream has a single recipient. Using end-to-end bandwidth estimation, the target bitrate can thus be well tailored for the intended recipient. The number of key frames can be kept to a minimum and the compressability of the stream can be maximized. One way of achiving this is by using "single-layer coding" (or "IPPP" encoding), where each delta frame only depends on the previous neighbouring delta frame.

### Scalable video coding
In multiway conferences, on the other hand, the encoded stream has multiple recipients that may have different downlink bandwidths. In order to tailor the encoded streams to these heterogeneous receivers, [scalable video coding][svc-wiki] can be used. The idea is to introduce structure into the dependency graph of the encoded stream, such that _layers_ of the full stream can be independently decoded. These layers allow for a [selective forwarding unit][sfu-webrtc-glossary] to discard parts of the stream, depending on the particular receiver's downlink bandwidth conditions.

There are multiple types of scalability:
* _Temporal scalability_ are layers whose framerate (and bitrate) is lower than that of the top layer
* _Spatial scalability_ are layers whose resolution (and bitrate) is lower than that of the top layer
* _Quality scalability_ are layers whose bitrate is lower than that of the top layer

WebRTC supports temporal scalability (`VP8`, `VP9`, `AV1`) and spatial scalability (`VP9`, `AV1`), but not quality scalability.

### Simulcast
Simulcast is another approach for multiway conferencing, where multiple _independent_ bitstreams are produced by the encoder.

Compared to using spatial scalability with inter-layer prediction, simulcast may provide a lower efficiency for the uplink, but may also provide a better efficiency on the downlink. The `K-SVC` concept, where spatial scalability is only applied to key frames, can be seen as a compromise between full spatial scalability and simulcast.

## Overview of implementation in `modules/video_coding`
Given the general introduction to video coding above, we now describe some specifics of the `modules/video_coding` folder in WebRTC.

### Built-in software codecs in `modules/video_coding/codecs`
This folder contains WebRTC-specific classes that wrap software codec implementations for different video coding standards:
* [libaom][libaom-src] for [AV1][av1-spec]
* [libvpx][libvpx-src] for [VP8][vp8-spec] and [VP9][vp9-spec]
* [OpenH264][openh264-src] for [H.264 constrained baseline profile][h264-spec]

Users of the library can also inject their own codecs, using the [VideoEncoderFactory][video-encoder-factory-interface] and [VideoDecoderFactory][video-decoder-factory-interface] interfaces. This is how platform-supported codecs, such as hardware backed codecs, are implemented.

### Video codec test framework in `modules/video_coding/codecs/test`
This folder contains a test framework that can be used to evaluate video quality performance of different video codec implementations.

### SVC helper classes in `modules/video_coding/svc`
* `ScalabilityStructure*` - different [standardized scalability structures][scalability-structure-spec]
* `ScalableVideoController` - provides instructions to the video encoder how to create a scalable stream
* `SvcRateAllocator` - bitrate allocation to different spatial and temporal layers

### Utility classes in `modules/video_coding/utility`
* `FrameDropper` - drops incoming frames when encoder systematically overshoots its target bitrate
* `FramerateController` - drops incoming frames to achieve a target framerate
* `QpParser` - parses the quantization parameter from a bitstream
* `QualityScaler` - signals when an encoder generates encoded frames whose quantization parameter is outside the window of acceptable values
* `SimulcastrateAllocator` - bitrate allocation to simulcast layers

### General helper classes in `modules/video_coding`
* `FecControllerDefault` - provides a default implementation for rate allocation to [forward error correction][fec-wiki]
* `VideoCodecInitializer` - converts between different encoder configuration structs

### Receiver buffer classes in `modules/video_coding`
* `PacketBuffer` - provides list of RTP packets that belong to frames
* `RtpFrameReferenceFinder` - provides frames for which we have all RTP packets and all references frames are known
* `FrameBuffer` - provides decodable frames to be fed to the decoder

[video-coding-wiki]: https://en.wikipedia.org/wiki/Video_coding_format
[motion-compensation-wiki]: https://en.wikipedia.org/wiki/Motion_compensation
[transform-coding-wiki]: https://en.wikipedia.org/wiki/Transform_coding
[motion-vector-wiki]: https://en.wikipedia.org/wiki/Motion_vector
[mpeg-wiki]: https://en.wikipedia.org/wiki/Moving_Picture_Experts_Group
[svc-wiki]: https://en.wikipedia.org/wiki/Scalable_Video_Coding
[sfu-webrtc-glossary]: https://webrtcglossary.com/sfu/
[libvpx-src]: https://chromium.googlesource.com/webm/libvpx/
[libaom-src]: https://aomedia.googlesource.com/aom/
[openh264-src]: https://github.com/cisco/openh264
[vp8-spec]: https://tools.ietf.org/html/rfc6386
[vp9-spec]: https://storage.googleapis.com/downloads.webmproject.org/docs/vp9/vp9-bitstream-specification-v0.6-20160331-draft.pdf
[av1-spec]: https://aomediacodec.github.io/av1-spec/
[h264-spec]: https://www.itu.int/rec/T-REC-H.264-201906-I/en
[video-encoder-factory-interface]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/video_codecs/video_encoder_factory.h;l=27;drc=afadfb24a5e608da6ae102b20b0add53a083dcf3
[video-decoder-factory-interface]: https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/api/video_codecs/video_decoder_factory.h;l=27;drc=49c293f03d8f593aa3aca282577fcb14daa63207
[scalability-structure-spec]: https://w3c.github.io/webrtc-svc/#scalabilitymodes*
[fec-wiki]: https://en.wikipedia.org/wiki/Error_correction_code#Forward_error_correction
[entropy-coding-wiki]: https://en.wikipedia.org/wiki/Entropy_encoding
