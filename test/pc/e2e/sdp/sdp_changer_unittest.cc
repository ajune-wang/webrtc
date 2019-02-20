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

#include <utility>

#include "absl/memory/memory.h"
#include "api/jsep.h"
#include "api/jsep_session_description.h"
#include "pc/session_description.h"
#include "pc/webrtc_sdp.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

constexpr char kOriginalSdpStartString[] =
    "v=0\r\n"
    "o=- 5140783666625180755 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0\r\n"
    "a=msid-semantic: WMS\r\n";

// VP8 codec has id 96 and its rtx has id 97
// VP9 codec has id 98 and its rtx has id 99
// Video string with first VP8 with its rtx and then VP9 with its rtx
constexpr char kVp8ThenVp9SdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102\r\n";
// Video string with first VP9 with its rtx and then VP8 with its rtx
constexpr char kVp9ThenVp8SdpVideoString[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 98 99 96 97 100 101 102\r\n";
// SdpChanger will push up only specified codec without its rtx, so in resulted
// string VP8 or VP9 codec will be put on the first place
// Video string with VP9 pushed on the 1st place
constexpr char kVp9PushedInfrontOfVp8[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 98 96 97 99 100 101 102\r\n";
// Video string with VP8 pushed on the 1st place
constexpr char kVp8PushedInfrontOfVp9[] =
    "m=video 9 UDP/TLS/RTP/SAVPF 96 98 99 97 100 101 102\r\n";

constexpr char kOriginalSdpEndString[] =
    "c=IN IP4 0.0.0.0\r\n"
    "a=rtcp:9 IN IP4 0.0.0.0\r\n"
    "a=ice-ufrag:AgVU\r\n"
    "a=ice-pwd:4a0yoyU1mZ+1mNN390B2Y/IP\r\n"
    "o=- 8977467767436087709 2 IN IP4 127.0.0.1\r\n"
    "a=ice-options:trickle\r\n"
    "a=fingerprint:sha-256 "
    "EF:F2:16:65:0C:C6:F6:5E:AE:DB:42:0C:39:1C:55:BB:1E:03:98:1E:D2:B1:E8:F4:"
    "8F:01:F9:55:99:53:C4:0A\r\n"
    "a=setup:actpass\r\n"
    "a=mid:0\r\n"
    "a=extmap:14 urn:ietf:params:rtp-hdrext:toffset\r\n"
    "a=extmap:13 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
    "a=extmap:12 urn:3gpp:video-orientation\r\n"
    "a=extmap:11 "
    "http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
    "a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
    "a=extmap:6 "
    "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\r\n"
    "a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing\r\n"
    "a=extmap:8 "
    "http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07\r\n"
    "a=extmap:9 http://www.webrtc.org/experiments/rtp-hdrext/color-space\r\n"
    "a=extmap:2 urn:ietf:params:rtp-hdrext:sdes:mid\r\n"
    "a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id\r\n"
    "a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id\r\n"
    "a=recvonly\r\n"
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
    "a=rtpmap:100 red/90000\r\n"
    "a=rtpmap:101 rtx/90000\r\n"
    "a=fmtp:101 apt=100\r\n"
    "a=rtpmap:102 ulpfec/90000\r\n"
    "a=ssrc-group:FID 4292973779 2350371325\r\n"
    "a=ssrc:4292973779 cname:7+WuOhvSmcP4VWTU\r\n"
    "a=ssrc:4292973779 msid:- alice-video\r\n"
    "a=ssrc:4292973779 mslabel:-\r\n"
    "a=ssrc:4292973779 label:alice-video\r\n"
    "a=ssrc:2350371325 cname:7+WuOhvSmcP4VWTU\r\n"
    "a=ssrc:2350371325 msid:- alice-video\r\n"
    "a=ssrc:2350371325 mslabel:-\r\n"
    "a=ssrc:2350371325 label:alice-video\r\n";

std::string GetVp8ThenVp9SdpString() {
  rtc::StringBuilder out;
  out << kOriginalSdpStartString << kVp8ThenVp9SdpVideoString
      << kOriginalSdpEndString;
  return out.str();
}

std::string GetVp9ThenVp8SdpString() {
  rtc::StringBuilder out;
  out << kOriginalSdpStartString << kVp9ThenVp8SdpVideoString
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
  return result ? std::move(desc) : nullptr;
}

}  // namespace

TEST(SdpChangerTest, ForceVideoCodecVP8WhenItIsAlreadySelected) {
  std::unique_ptr<SessionDescriptionInterface> original_sdp =
      ParseSdp(GetVp8ThenVp9SdpString());
  ASSERT_TRUE(original_sdp != nullptr);

  SdpChanger changer(std::move(original_sdp));
  changer.ForceVideoCodec("alice-video", "VP8");
  std::unique_ptr<SessionDescriptionInterface> patched_sdp =
      changer.ReleaseSessionDescription();

  std::string patched_sdp_string = SdpToString(patched_sdp.get());
  // VP8 is already on the first place, so order have to be the same.
  std::string expected_video_line = kVp8ThenVp9SdpVideoString;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceVideoCodecVP9) {
  std::unique_ptr<SessionDescriptionInterface> original_sdp =
      ParseSdp(GetVp8ThenVp9SdpString());
  ASSERT_TRUE(original_sdp != nullptr);

  SdpChanger changer(std::move(original_sdp));
  changer.ForceVideoCodec("alice-video", "VP9");
  std::unique_ptr<SessionDescriptionInterface> patched_sdp =
      changer.ReleaseSessionDescription();

  std::string patched_sdp_string = SdpToString(patched_sdp.get());
  // VP9 have to be moved on the first place
  std::string expected_video_line = kVp9PushedInfrontOfVp8;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

TEST(SdpChangerTest, ForceVideoCodecVP8) {
  std::unique_ptr<SessionDescriptionInterface> original_sdp =
      ParseSdp(GetVp9ThenVp8SdpString());
  ASSERT_TRUE(original_sdp != nullptr);

  SdpChanger changer(std::move(original_sdp));
  changer.ForceVideoCodec("alice-video", "VP8");
  std::unique_ptr<SessionDescriptionInterface> patched_sdp =
      changer.ReleaseSessionDescription();

  std::string patched_sdp_string = SdpToString(patched_sdp.get());
  // VP8 have to be moved on the first place
  std::string expected_video_line = kVp8PushedInfrontOfVp9;
  std::size_t found = patched_sdp_string.find(expected_video_line);
  EXPECT_NE(found, std::string::npos) << patched_sdp_string;
}

}  // namespace test
}  // namespace webrtc
