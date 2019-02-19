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

#include <set>
#include <utility>
#include <vector>

#include "api/media_types.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "pc/session_description.h"

namespace webrtc {
namespace test {

SdpChanger::SdpChanger(
    std::unique_ptr<SessionDescriptionInterface> session_description)
    : session_description_(std::move(session_description)) {
  RTC_CHECK(session_description_);
}
SdpChanger::~SdpChanger() = default;

void SdpChanger::ForceVideoCodec(const std::string& stream_label,
                                 std::string codec_name) {
  RTC_CHECK(session_description_);
  bool stream_found = false;
  for (cricket::ContentInfo& content :
       session_description_->description()->contents()) {
    if (content.media_description()->type() !=
        cricket::MediaType::MEDIA_TYPE_VIDEO) {
      continue;
    }
    for (const cricket::StreamParams& stream :
         content.media_description()->as_video()->streams()) {
      if (stream.id == stream_label) {
        stream_found = true;
        break;
      }
    }
    if (!stream_found) {
      continue;
    }

    cricket::VideoContentDescription* description =
        content.media_description()->as_video();

    std::vector<cricket::VideoCodec> codecs = description->codecs();
    bool required_codec_found = false;
    // Find required codec by name and put it on the first place in codecs list.
    for (auto it = codecs.begin(); it < codecs.end();) {
      if (it->name == codec_name) {
        required_codec_found = true;
        cricket::VideoCodec required_codec = *it;
        codecs.erase(it);
        codecs.insert(codecs.begin(), required_codec);
        break;
      }
    }
    RTC_CHECK(required_codec_found)
        << "Codec_name=" << codec_name
        << " is unsupported for this peer connection";

    description->set_codecs(codecs);
  }
  RTC_CHECK(stream_found) << "No stream with stream_label=" << stream_label;
}

std::unique_ptr<SessionDescriptionInterface>
SdpChanger::ReleaseSessionDescription() {
  RTC_CHECK(session_description_);
  return std::move(session_description_);
}

}  // namespace test
}  // namespace webrtc
