<?% config.freshness.owner = 'brandtr' %?>
<?% config.freshness.reviewed = '2021-04-13' %?>
# Video coding in WebRTC

## Introduction to video coding and layered coding
[Video coding](video-coding-wiki) is about encoding a stream of raw video frames into a compressed bitstream, whose bitrate is lower than that of the original stream.

### Motion compensated transform coding
All video codecs in WebRTC are based on the [motion compensated](motion-compensation-wiki) [transform coding](transform-coding-wiki) paradigm. This means that each video frame is divided into a distinct set of pixel blocks, whose contents can be motion compensated. The resulting residuals are transform coded. Finally, the motion vectors and quantized residuals are further compressed using a lossless entropy coder.

On the receiving end, the original video frames are reconstructed using the motion information and quantized residuals.

### Frame types
When an encoded frame depends on a previously encoded frame (inter-frame dependencies), the prior frames must be available at the receiver before the current frame can be decoded. In order for a new receiver to start decoding an encoded stream, a frame which has no prior dependencies is thus needed. Such a frame is called a "key frame" (or "I-frame" using [MPEG](mpeg-wiki) terminology). Such frames typically compress more poorly than "delta frames" (or "P-frames"). For real-time communication purposes, we thus try to limit the number of keyframes generated to be a minimum.

### Single-layer encoding
In 1:1 calls, the encoded stream has a single recipient, and the target bitrate of the encoded stream can thus be perfectly tailored for the intended recipient using end-to-end bandwidth estimation. Unless decodability is completely lost at the receiver, the number of keyframes can be kept to a minimum and the compressability of the stream can be maximized. One way of achiving this is by using "single-layer encoding" (or "IPPP" encoding), where each delta frame only depends on the previous delta frame.

### Scalable video coding
In multiway conferences, on the other hand, the encoded stream may have multiple recipients with different downlink bandwidths. In order to tailor the encoded streams to these heterogeneous receivers, the [scalable video coding](svc-wiki) video coding approach can be applied. The idea is to introduce structure into the dependency graph of the encoded stream, such that layers of the full stream can be independently decoded. This allows for a [selective forwarding unit](sfu-webrtc-glossary) to discard parts of the stream, for the receivers that are restricted by low downlink bandwidths.

There are multiple types of scalability:
* _Temporal scalability_ produces layers whose frame rate is lower than the top layer.
* _Spatial scalability_ produces layers whose resolution is lower than the top layer.
* _Quality scalability_ produces layers whose bit rate is lower than the top layer.

WebRTC supports temporal (`VP8`, `VP9`, `AV1`) and spatial scalability (`VP9`, `AV1`), but not quality scalability.

### Simulcast
Simulcast is another approach, where multiple independent bitstreams are produced by the encoder, without any scalability applied. Compared to using spatial scalability with inter-layer prediction, simulcast may provide a lower efficiency for the uplink, but may also provide a better efficiency on the downlink.

The concept of `k-SVC`, where spatial scalability is only applied to the key frames, can be seen as a compromise between full spatial scalability and full simulcast.

## Overview of `modules/video_coding`
Given the general introduction to video coding above, we now describe the specifics of video coding in WebRTC.



This folder contains supporting fucntionality for software-backed codecs, video coding-related helper classes, as well as the receiving jitter buffer.

### Built-in software codecs in `modules/video_coding/codecs`
The software codec implementations of the different video coding standards are:
* [libaom](libaom-src) for [AV1](av1-spec)
* [libvpx](libvpx-src) for [VP8](vp8-spec) and [VP9](vp9-spec)
* [OpenH264](openh264-src) for [H.264 constrained baseline](h264-spec)

Users of the library can also inject their own codecs, using the [VideoEncoderFactory](video-encoder-factory-interface) and [VideoDecoderFactory](video-decoder-factory-interface). This is how platform-supported codecs (such as hardware backed codecs) are implemented.

### Video codec test framework in `modules/video_coding/codecs/test`
This is a test framework that can be used to evaluate video quality performance and complexity of different video codec implementations.

### SVC support in `modules/video_coding/svc`
This folder contains helper classes for implementing SVC:
* `ScalabilityStructure*` - different [scalability structures](scalability-structure-spec)
* `ScalableVideoController` - provides instructions to the video encoder how to create a scalable stream
* `SvcRateAllocator` - how to allocate bit rate to different layers

### Utility classes in `modules/video_coding/utility`
* `FrameDropper` - drops frames when encoder overshoots
* `FramerateController` - drops frames when configured to do so
* `QpParser` - parses the quantization parameter from a compressed bitstream
* `QualityScaler` - measured the QP and signals when it overshoots/undershoots
* `SimulcastrateAllocator` - allocates bitrate to a number of indenpenetn simulcast layer
* `Vp8HeaderParser` - Parses the frame header in p8
* `VP9UncompressedHeader` - Parses the uncompressed frame header in vp9

### General helper classes in `modules/video_coding`
* `FecControllerDefault` - provides a default implementation for forward error correction rate allocation
* `VideoCodecInitializer` - converts between different encoder configuration structs.

#### Receiver buffer classes in `modules/video_coding`
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
