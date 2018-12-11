/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/rtp_dump_replayer.h"

namespace webrtc {
namespace {

// Configuration is directly coded into the file to prevent disk access during
// fuzzing which requires the ability to fork at high speed.
constexpr char kConfiguration[] =
    "["
    "   {"
    "      \"decoders\" : ["
    "         {"
    "            \"codec_params\" : [],"
    "            \"payload_name\" : \"VP9\","
    "            \"payload_type\" : 98"
    "         },"
    "         {"
    "            \"codec_params\" : [],"
    "            \"payload_name\" : \"VP8\","
    "            \"payload_type\" : 96"
    "         }"
    "      ],"
    "      \"render_delay_ms\" : 10,"
    "      \"rtp\" : {"
    "         \"extensions\" : ["
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 5,"
    "               \"uri\" : "
    "\"http://www.ietf.org/id/"
    "draft-holmer-rmcat-transport-wide-cc-extensions-01\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 3,"
    "               \"uri\" : "
    "\"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 6,"
    "               \"uri\" : "
    "\"http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 7,"
    "               \"uri\" : "
    "\"http://www.webrtc.org/experiments/rtp-hdrext/video-content-type\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 8,"
    "               \"uri\" : "
    "\"http://www.webrtc.org/experiments/rtp-hdrext/video-timing\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 4,"
    "               \"uri\" : \"urn:3gpp:video-orientation\""
    "            },"
    "            {"
    "               \"encrypt\" : false,"
    "               \"id\" : 2,"
    "               \"uri\" : \"urn:ietf:params:rtp-hdrext:toffset\""
    "            }"
    "         ],"
    "         \"local_ssrc\" : 1,"
    "         \"nack\" : {"
    "            \"rtp_history_ms\" : 1000"
    "         },"
    "         \"red_payload_type\" : -1,"
    "         \"remb\" : true,"
    "         \"remote_ssrc\" : 2678204013,"
    "         \"rtcp_mode\" : \"RtcpMode::kReducedSize\","
    "         \"rtx_payload_types\" : ["
    "            {"
    "               \"97\" : 96"
    "            },"
    "            {"
    "               \"99\" : 98"
    "            }"
    "         ],"
    "         \"rtx_ssrc\" : 1110725867,"
    "         \"transport_cc\" : true,"
    "         \"ulpfec_payload_type\" : -1"
    "      },"
    "      \"target_delay_ms\" : 0"
    "   }"
    "]";

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  rtc::ArrayView<const uint8_t> rtp_dump(data, size);
  auto stream_state =
      test::RtpDumpReplayer::StreamState::FromString(kConfiguration);
  if (stream_state != nullptr) {
    test::RtpDumpReplayer::Replay(std::move(stream_state), rtp_dump);
  }
}

}  // namespace webrtc
