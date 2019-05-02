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
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

const std::set<std::string>* GetKnownCodecNames() {
  static const std::set<std::string>* const known_codec_names =
      new std::set<std::string>({cricket::kVp8CodecName, cricket::kVp9CodecName,
                                 cricket::kH264CodecName});
  return known_codec_names;
}

std::string CodecRequiredParamsToString(
    const std::map<std::string, std::string>& codec_required_params) {
  rtc::StringBuilder out;
  for (auto entry : codec_required_params) {
    out << entry.first << "=" << entry.second << ";";
  }
  return out.str();
}

}  // namespace

void ForceVideoCodec(
    SessionDescriptionInterface* session_description,
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params) {
  RTC_CHECK(session_description);
  RTC_CHECK(session_description->description());
  for (cricket::ContentInfo& content :
       session_description->description()->contents()) {
    if (content.media_description()->type() !=
        cricket::MediaType::MEDIA_TYPE_VIDEO) {
      continue;
    }

    cricket::VideoContentDescription* description =
        content.media_description()->as_video();
    RTC_DCHECK(description);

    std::vector<cricket::VideoCodec> codecs = description->codecs();
    // We want to have support for these options:
    //  1. Specify one of the known codecs to use (one from
    //  GetKnownCodecNames())
    //  2. Provide own codec and use it.
    // We will assume, that SDP contains:
    //  * known codecs
    //  * retransmission (RTX) "codecs" for codecs
    //  * FEC "codecs"
    //  * some extract support "codec"
    //  * probably user provided codecs
    // To force codec |codec_name| we need to put it on the first place in the
    // list and filter out all other real codecs, but keep retransmission, FEC
    // and other support "codecs". To achieve it we will filter out all known
    // codecs except the one with |codec_name|, then we will put it on the first
    // place.

    // Remove irrelevant codecs
    const std::set<std::string>* known_codecs = GetKnownCodecNames();
    codecs.erase(
        std::remove_if(
            codecs.begin(), codecs.end(),
            [known_codecs, codec_name,
             codec_required_params](const cricket::VideoCodec& codec) {
              if (known_codecs->count(codec.name) == 0) {
                // If we don't know this codec, then we will keep it.
                return false;
              }
              if (codec.name != codec_name) {
                return true;
              }
              for (auto required_param_entry : codec_required_params) {
                std::string param_value;
                if (!codec.GetParam(required_param_entry.first, &param_value)) {
                  return true;
                }
                if (param_value != required_param_entry.second) {
                  return true;
                }
              }
              return false;
            }),
        codecs.end());
    // Remove rtx, that points to the removed codecs.
    std::set<std::string> presented_codec_ids;
    for (auto it = codecs.begin(); it != codecs.end(); ++it) {
      presented_codec_ids.insert(std::to_string(it->id));
    }
    codecs.erase(std::remove_if(
                     codecs.begin(), codecs.end(),
                     [presented_codec_ids](const cricket::VideoCodec& codec) {
                       std::string apt_param_value;
                       if (!codec.GetParam("apt", &apt_param_value)) {
                         return false;
                       }
                       return (presented_codec_ids.count(apt_param_value) == 0);
                     }),
                 codecs.end());
    // Put requested codec on the 1st place
    auto it = absl::c_find_if(codecs,
                              [&codec_name](const cricket::VideoCodec& codec) {
                                return codec.name == codec_name;
                              });
    RTC_CHECK(it != codecs.end())
        << "Codec with name=" << codec_name.data() << " and params {"
        << CodecRequiredParamsToString(codec_required_params)
        << "} is unsupported for this peer connection";
    std::rotate(codecs.begin(), it, it + 1);

    description->set_codecs(codecs);
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
