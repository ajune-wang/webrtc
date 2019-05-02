/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/sdp/sdp_changer.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/jsep.h"
#include "api/jsep_session_description.h"
#include "pc/session_description.h"
#include "pc/webrtc_sdp.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr char kOriginalSdpStartString[] =
    "v=0\r\n"
    "o=- 1618074677686351135 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1\r\n"
    "a=msid-semantic: WMS _auto_audio_stream_label_0 alice-video\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 102 0 8 106 105 13 110 112 113 "
    "126\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:HDmG\r\n"
    "a=ice-pwd:uQQt3EmLlqd9VVfROXxo9pF2\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "A8:71:95:A5:BD:9B:8C:E6:DB:0E:83:3B:EF:08:1C:3D:51:AD:FD:2C:2B:95:E5:1F:"
    "9B:50:8E:8D:07:78:4C:C7\r\n"
    "a=setup:actpass\r\n"
    "a=mid:0\r\n"
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
    "a=extmap:2 "
    "http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
    "a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n"
    "a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id\r\n"
    "a=sendrecv\r\n"
    "a=msid:_auto_audio_stream_label_0 e0b729dd-789f-4b36-af00-58f638806e21\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "a=rtcp-fb:111 transport-cc\r\n"
    "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=rtpmap:104 ISAC/32000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:102 ILBC/8000\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:106 CN/32000\r\n"
    "a=rtpmap:105 CN/16000\r\n"
    "a=rtpmap:13 CN/8000\r\n"
    "a=rtpmap:110 telephone-event/48000\r\n"
    "a=rtpmap:112 telephone-event/32000\r\n"
    "a=rtpmap:113 telephone-event/16000\r\n"
    "a=rtpmap:126 telephone-event/8000\r\n"
    "a=ssrc:4187388252 cname:HRjn0nG6zNqyBHJ6\r\n"
    "a=ssrc:4187388252 msid:_auto_audio_stream_label_0 "
    "e0b729dd-789f-4b36-af00-58f638806e21\r\n"
    "a=ssrc:4187388252 mslabel:_auto_audio_stream_label_0\r\n"
    "a=ssrc:4187388252 label:e0b729dd-789f-4b36-af00-58f638806e21\r\n";

// Video codecs:
// 96 97 - VP8 and rtx layer
// 98 99 - VP9 profile-id=0 and rtx layer
// 100 101 - VP9 profile-id=2 and rtx layer
// 127 121 - H264 level-asymmetry-allowed=1; packetization-mode=1;
//           profile-level-id=42001f and rtx layer
// 125 120 - H264 level-asymmetry-allowed=1; packetization-mode=0;
//           profile-level-id=42001f and rtx layer
// 124 107 - H264 level-asymmetry-allowed=1; packetization-mode=1;
//           profile-level-id=42e01f and rtx layer
// 108 109 - H264 level-asymmetry-allowed=1; packetization-mode=0;
//           profile-level-id=42e01f and rtx layer
// 123 119 - red and rtx layer
// 122 - ulpfec
// Original sdp video string with all video codecs presented.
constexpr char kOriginalSdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 121 125 120 124 107 "
    "108 109 123 119 122\r\n";

// Video string with VP8 and its rtx.
constexpr char kVp8SdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 123 119 122\r\n";
// Video string with VP9 and its rtx.
constexpr char kVp9SdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 98 99 100 101 123 119 122\r\n";
// Video string with all H264 and their rtx in original order.
constexpr char kH264AllSdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 127 121 125 120 124 107 108 109 123 119 "
    "122\r\n";
// Video string with H264 packetization-mode=1; profile-level-id=42e01f and its
// rtx.
constexpr char kH264WithProfile42e01fPacketizationMode1SdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 124 107 123 119 122\r\n";
// Video string with ulpfec first.
constexpr char kUnknownVideoCodecSdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 122 123 119\r\n";

constexpr char kOriginalSdpEndString[] =
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:HDmG\r\n"
    "a=ice-pwd:uQQt3EmLlqd9VVfROXxo9pF2\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "A8:71:95:A5:BD:9B:8C:E6:DB:0E:83:3B:EF:08:1C:3D:51:AD:FD:2C:2B:95:E5:1F:"
    "9B:50:8E:8D:07:78:4C:C7\r\n"
    "a=setup:actpass\r\n"
    "a=mid:1\r\n"
    "a=extmap:14 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:13 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:12 urn:3gpp:video-orientation\r\n"
    "a=extmap:2 "
    "http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
    "a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
    "a=extmap:6 "
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\r\n"
    "a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing\r\n"
    "a=extmap:8 "
    "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07\r\n"
    "a=extmap:9 http://www.webrtc.org/experiments/rtp-hdrext/color-space\r\n"
    "a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n"
    "a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id\r\n"
    "a=sendrecv\r\n"
    "a=msid:alice-video ccc9dd60-b043-40d5-972c-cc058fcffb5d\r\n"
    "a=rtcp-mux\r\n"
    "a=rtcp-rsize\r\n"
    "a=rtpmap:96 VP8/90000\r\n"
    "a=rtcp-fb:96 goog-remb\r\n"
    "a=rtcp-fb:96 transport-cc\r\n"
    "a=rtcp-fb:96 ccm fir\r\n"
    "a=rtcp-fb:96 nack\r\n"
    "a=rtcp-fb:96 nack pli\r\n"
    "a=rtpmap:97 rtx/90000\r\n"
    "a=fmtp:97 apt=96\r\n"
    "a=rtpmap:98 VP9/90000\r\n"
    "a=rtcp-fb:98 goog-remb\r\n"
    "a=rtcp-fb:98 transport-cc\r\n"
    "a=rtcp-fb:98 ccm fir\r\n"
    "a=rtcp-fb:98 nack\r\n"
    "a=rtcp-fb:98 nack pli\r\n"
    "a=fmtp:98 profile-id=0\r\n"
    "a=rtpmap:99 rtx/90000\r\n"
    "a=fmtp:99 apt=98\r\n"
    "a=rtpmap:100 VP9/90000\r\n"
    "a=rtcp-fb:100 goog-remb\r\n"
    "a=rtcp-fb:100 transport-cc\r\n"
    "a=rtcp-fb:100 ccm fir\r\n"
    "a=rtcp-fb:100 nack\r\n"
    "a=rtcp-fb:100 nack pli\r\n"
    "a=fmtp:100 profile-id=2\r\n"
    "a=rtpmap:101 rtx/90000\r\n"
    "a=fmtp:101 apt=100\r\n"
    "a=rtpmap:127 H264/90000\r\n"
    "a=rtcp-fb:127 goog-remb\r\n"
    "a=rtcp-fb:127 transport-cc\r\n"
    "a=rtcp-fb:127 ccm fir\r\n"
    "a=rtcp-fb:127 nack\r\n"
    "a=rtcp-fb:127 nack pli\r\n"
    "a=fmtp:127 "
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f\r\n"
    "a=rtpmap:121 rtx/90000\r\n"
    "a=fmtp:121 apt=127\r\n"
    "a=rtpmap:125 H264/90000\r\n"
    "a=rtcp-fb:125 goog-remb\r\n"
    "a=rtcp-fb:125 transport-cc\r\n"
    "a=rtcp-fb:125 ccm fir\r\n"
    "a=rtcp-fb:125 nack\r\n"
    "a=rtcp-fb:125 nack pli\r\n"
    "a=fmtp:125 "
    "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f\r\n"
    "a=rtpmap:120 rtx/90000\r\n"
    "a=fmtp:120 apt=125\r\n"
    "a=rtpmap:124 H264/90000\r\n"
    "a=rtcp-fb:124 goog-remb\r\n"
    "a=rtcp-fb:124 transport-cc\r\n"
    "a=rtcp-fb:124 ccm fir\r\n"
    "a=rtcp-fb:124 nack\r\n"
    "a=rtcp-fb:124 nack pli\r\n"
    "a=fmtp:124 "
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
    "a=rtpmap:107 rtx/90000\r\n"
    "a=fmtp:107 apt=124\r\n"
    "a=rtpmap:108 H264/90000\r\n"
    "a=rtcp-fb:108 goog-remb\r\n"
    "a=rtcp-fb:108 transport-cc\r\n"
    "a=rtcp-fb:108 ccm fir\r\n"
    "a=rtcp-fb:108 nack\r\n"
    "a=rtcp-fb:108 nack pli\r\n"
    "a=fmtp:108 "
    "level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f\r\n"
    "a=rtpmap:109 rtx/90000\r\n"
    "a=fmtp:109 apt=108\r\n"
    "a=rtpmap:123 red/90000\r\n"
    "a=rtpmap:119 rtx/90000\r\n"
    "a=fmtp:119 apt=123\r\n"
    "a=rtpmap:122 ulpfec/90000\r\n"
    "a=ssrc-group:FID 3990736550 4041123556\r\n"
    "a=ssrc:3990736550 cname:HRjn0nG6zNqyBHJ6\r\n"
    "a=ssrc:3990736550 msid:alice-video "
    "ccc9dd60-b043-40d5-972c-cc058fcffb5d\r\n"
    "a=ssrc:3990736550 mslabel:alice-video\r\n"
    "a=ssrc:3990736550 label:ccc9dd60-b043-40d5-972c-cc058fcffb5d\r\n"
    "a=ssrc:4041123556 cname:HRjn0nG6zNqyBHJ6\r\n"
    "a=ssrc:4041123556 msid:alice-video "
    "ccc9dd60-b043-40d5-972c-cc058fcffb5d\r\n"
    "a=ssrc:4041123556 mslabel:alice-video\r\n"
    "a=ssrc:4041123556 label:ccc9dd60-b043-40d5-972c-cc058fcffb5d\r\n";

std::string GetOriginalSdpString() {
  rtc::StringBuilder out;
  out << kOriginalSdpStartString << kOriginalSdpVideoString
      << kOriginalSdpEndString;
  return out.str();
}

std::string SdpToString(SessionDescriptionInterface* sdp) {
  std::string out;
  sdp->ToString(&out);
  return out;
}

std::unique_ptr<SessionDescriptionInterface> ParseSdp(std::string sdp_string) {
  auto desc = absl::make_unique<JsepSessionDescription>(SdpType::kOffer);
  SdpParseError error;
  bool result = webrtc::SdpDeserialize(sdp_string, desc.get(), &error);
  EXPECT_TRUE(result) << error.line << ": " << error.description;
  return result ? std::move(desc) : nullptr;
}

}  // namespace

TEST(SdpChangerTest, ForceVideoCodecVP8) {
  std::unique_ptr<SessionDescriptionInterface> sdp =
      ParseSdp(GetOriginalSdpString());
  ASSERT_TRUE(sdp != nullptr);

  ForceVideoCodec(sdp.get(), "VP8", {});

  std::string patched_sdp_string = SdpToString(sdp.get());
  std::string expected_video_line = kVp8SdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceVideoCodecVP9) {
  std::unique_ptr<SessionDescriptionInterface> sdp =
      ParseSdp(GetOriginalSdpString());
  ASSERT_TRUE(sdp != nullptr);

  ForceVideoCodec(sdp.get(), "VP9", {});

  std::string patched_sdp_string = SdpToString(sdp.get());
  std::string expected_video_line = kVp9SdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceVideoCodecH264All) {
  std::unique_ptr<SessionDescriptionInterface> sdp =
      ParseSdp(GetOriginalSdpString());
  ASSERT_TRUE(sdp != nullptr);

  ForceVideoCodec(sdp.get(), "H264", {});

  std::string patched_sdp_string = SdpToString(sdp.get());
  std::string expected_video_line = kH264AllSdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceVideoCodecH264WithProfile42e01fPacketizationMode1) {
  std::unique_ptr<SessionDescriptionInterface> sdp =
      ParseSdp(GetOriginalSdpString());
  ASSERT_TRUE(sdp != nullptr);

  ForceVideoCodec(
      sdp.get(), "H264",
      {{"profile-level-id", "42e01f"}, {"packetization-mode", "1"}});

  std::string patched_sdp_string = SdpToString(sdp.get());
  std::string expected_video_line =
      kH264WithProfile42e01fPacketizationMode1SdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceUnknownVideoCodec) {
  std::unique_ptr<SessionDescriptionInterface> sdp =
      ParseSdp(GetOriginalSdpString());
  ASSERT_TRUE(sdp != nullptr);

  ForceVideoCodec(sdp.get(), "ulpfec", {});

  std::string patched_sdp_string = SdpToString(sdp.get());
  std::string expected_video_line = kUnknownVideoCodecSdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
