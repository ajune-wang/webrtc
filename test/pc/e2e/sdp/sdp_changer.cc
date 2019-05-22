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
#include "api/jsep_session_description.h"
#include "media/base/media_constants.h"
#include "p2p/base/p2p_constants.h"
#include "pc/sdp_utils.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

std::string CodecRequiredParamsToString(
    const std::map<std::string, std::string>& codec_required_params) {
  rtc::StringBuilder out;
  for (auto entry : codec_required_params) {
    out << entry.first << "=" << entry.second << ";";
  }
  return out.str();
}

}  // namespace

std::vector<RtpCodecCapability> FilterCodecCapabilities(
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params,
    bool ulpfec,
    bool flexfec,
    std::vector<RtpCodecCapability> supported_codecs) {
  std::vector<RtpCodecCapability> output_codecs;
  // Find main requested codecs among supported and add them to output.
  for (auto& codec : supported_codecs) {
    if (codec.name != codec_name) {
      continue;
    }
    bool parameters_matched = true;
    for (auto item : codec_required_params) {
      auto it = codec.parameters.find(item.first);
      if (it == codec.parameters.end()) {
        parameters_matched = false;
        break;
      }
      if (item.second != it->second) {
        parameters_matched = false;
        break;
      }
    }
    if (parameters_matched) {
      output_codecs.push_back(codec);
    }
  }

  RTC_CHECK_GT(output_codecs.size(), 0)
      << "Codec with name=" << codec_name << " and params {"
      << CodecRequiredParamsToString(codec_required_params)
      << "} is unsupported for this peer connection";

  // Add required FEC and RTX codecs to output.
  for (auto& codec : supported_codecs) {
    if (codec.name == cricket::kRtxCodecName) {
      output_codecs.push_back(codec);
    } else if (codec.name == cricket::kFlexfecCodecName && flexfec) {
      output_codecs.push_back(codec);
    } else if ((codec.name == cricket::kRedCodecName ||
                codec.name == cricket::kUlpfecCodecName) &&
               ulpfec) {
      // Red and ulpfec should be enabled or disabled together.
      output_codecs.push_back(codec);
    }
  }
  return output_codecs;
}

// If offer has no simulcast video sections - do nothing and returns empty
// OfferAnswerExchangeContext.
//
// If offer has simulcast video sections - for each section creates
// SimulcastSectionInfo and update section's rids to be unique with all sections
// mids.
OfferAnswerExchangeContext PrepareSimulcastOffer(
    SessionDescriptionInterface* offer) {
  OfferAnswerExchangeContext context;
  for (auto& content : offer->description()->contents()) {
    cricket::MediaContentDescription* media_desc = content.media_description();
    if (media_desc->type() != cricket::MediaType::MEDIA_TYPE_VIDEO) {
      continue;
    }
    if (media_desc->HasSimulcast()) {
      // We support only single stream simulcast sections with rids.
      RTC_CHECK_EQ(media_desc->mutable_streams().size(), 1);
      RTC_CHECK(media_desc->mutable_streams()[0].has_rids());

      // Create SimulcastSectionInfo for this video section.
      SimulcastSectionInfo info(content.mid(), content.type,
                                media_desc->mutable_streams()[0].rids());

      // Set new rids basing on created SimulcastSectionInfo.
      std::vector<cricket::RidDescription> rids;
      cricket::SimulcastDescription simulcast_description;
      for (std::string& rid : info.rids) {
        rids.emplace_back(rid, cricket::RidDirection::kSend);
        simulcast_description.send_layers().AddLayer(
            cricket::SimulcastLayer(rid, false));
      }
      media_desc->mutable_streams()[0].set_rids(rids);
      media_desc->set_simulcast_description(simulcast_description);

      info.simulcast_description = simulcast_description;
      for (auto extension : media_desc->rtp_header_extensions()) {
        if (extension.uri == RtpExtension::kRidUri) {
          info.rid_extension = extension;
        } else if (extension.uri == RtpExtension::kMidUri) {
          info.mid_extension = extension;
        }
      }
      RTC_CHECK_NE(info.rid_extension.id, 0);
      RTC_CHECK_NE(info.mid_extension.id, 0);
      bool transport_description_found = false;
      for (auto& transport_info : offer->description()->transport_infos()) {
        if (transport_info.content_name == info.mid) {
          info.transport_description = transport_info.description;
          transport_description_found = true;
        }
      }
      RTC_CHECK(transport_description_found);

      context.AddSimulcastInfo(info);
    }
  }
  return context;
}

PatchedOffer PatchOffer(SessionDescriptionInterface* offer) {
  OfferAnswerExchangeContext context = PrepareSimulcastOffer(offer);
  if (context.empty()) {
    return PatchedOffer(CloneSessionDescription(offer));
  }

  // Clone original offer description. We mustn't access original offer after
  // this point.
  std::unique_ptr<cricket::SessionDescription> desc =
      offer->description()->Clone();

  for (auto& info : context.simulcast_infos) {
    // For each simulcast section we have to perform:
    //   1. Swap MID and RID header extensions
    //   2. Remove RIDs from streams and remove SimulcastDescription
    //   3. For each RID duplicate media section
    cricket::ContentInfo* simulcast_content = desc->GetContentByName(info.mid);

    // Now we need to prepare common prototype for "m=video" sections, in which
    // single simulcast section will be converted. Do it before removing content
    // because otherwise description will be deleted.
    cricket::MediaContentDescription* prototype_media_desc =
        simulcast_content->media_description()->Copy();

    // Remove simulcast video section from offer.
    RTC_CHECK(desc->RemoveContentByName(simulcast_content->mid()));
    // Clear |simulcast_content|, because now it is pointing to removed object.
    simulcast_content = nullptr;

    // Swap mid and rid extensions, so remote peer will understand rid as mid.
    // Also remove rid extension.
    std::vector<webrtc::RtpExtension> extensions =
        prototype_media_desc->rtp_header_extensions();
    for (auto ext_it = extensions.begin(); ext_it != extensions.end();) {
      if (ext_it->uri == RtpExtension::kRidUri) {
        // We don't need rid extension for remote peer.
        extensions.erase(ext_it);
        continue;
      }
      if (ext_it->uri == RtpExtension::kMidUri) {
        ext_it->id = info.rid_extension.id;
      }
      ++ext_it;
    }
    prototype_media_desc->ClearRtpHeaderExtensions();
    prototype_media_desc->set_rtp_header_extensions(extensions);

    // We support only single stream inside video section with simulcast
    RTC_CHECK_EQ(prototype_media_desc->mutable_streams().size(), 1);
    // This stream must have rids.
    RTC_CHECK(prototype_media_desc->mutable_streams()[0].has_rids());

    // Remove rids and simulcast description from media description.
    prototype_media_desc->mutable_streams()[0].set_rids({});
    prototype_media_desc->set_simulcast_description(
        cricket::SimulcastDescription());

    // For each rid add separate video section.
    for (std::string& rid : info.rids) {
      desc->AddContent(rid, info.media_protocol_type,
                       prototype_media_desc->Copy());
    }
    delete prototype_media_desc;
  }

  // Now we need to add bundle line to have all media bundled together.
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  for (auto& content : desc->contents()) {
    bundle_group.AddContentName(content.mid());
  }
  if (desc->HasGroup(cricket::GROUP_TYPE_BUNDLE)) {
    desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  }
  desc->AddGroup(bundle_group);

  // Update transport_infos to add TransportInfo for each new media section.
  std::vector<cricket::TransportInfo> transport_infos = desc->transport_infos();
  for (auto info_it = transport_infos.begin();
       info_it != transport_infos.end();) {
    if (context.simulcast_infos_by_mid.find(info_it->content_name) !=
        context.simulcast_infos_by_mid.end()) {
      // Remove transport infos that correspond to simulcast video sections.
      transport_infos.erase(info_it);
    } else {
      ++info_it;
    }
  }
  for (auto& info : context.simulcast_infos) {
    for (auto& rid : info.rids) {
      transport_infos.emplace_back(rid, info.transport_description);
    }
  }
  desc->set_transport_infos(transport_infos);

  // Create patched offer.
  auto patched_offer =
      absl::make_unique<JsepSessionDescription>(SdpType::kOffer);
  patched_offer->Initialize(std::move(desc), offer->session_id(),
                            offer->session_version());
  return PatchedOffer(std::move(patched_offer), std::move(context));
}

std::unique_ptr<SessionDescriptionInterface> PatchAnswer(
    std::unique_ptr<SessionDescriptionInterface> answer,
    const OfferAnswerExchangeContext& context) {
  if (context.empty()) {
    return answer;
  }

  std::unique_ptr<cricket::SessionDescription> desc =
      answer->description()->Clone();

  for (auto& info : context.simulcast_infos) {
    cricket::ContentInfo* simulcast_content =
        desc->GetContentByName(info.rids[0]);
    RTC_CHECK(simulcast_content);

    // Get media description, which will be converted to simulcast answer.
    cricket::MediaContentDescription* media_desc =
        simulcast_content->media_description()->Copy();
    // Set |simulcast_content| to nullptr, because then it will be removed, so
    // it will point to deleted object.
    simulcast_content = nullptr;

    // Remove separate media sections for simulcast streams.
    for (auto& rid : info.rids) {
      RTC_CHECK(desc->RemoveContentByName(rid));
    }

    // Patch |media_desc| to make it simulcast answer description.
    // Restore mid/rid rtp header extensions
    std::vector<webrtc::RtpExtension> extensions =
        media_desc->rtp_header_extensions();
    // First remove existing rid/mid header extensions.
    for (auto ext_it = extensions.begin(); ext_it != extensions.end();) {
      if (ext_it->uri == RtpExtension::kRidUri ||
          ext_it->uri == RtpExtension::kMidUri) {
        extensions.erase(ext_it);
        continue;
      }
      ++ext_it;
    }
    // Then add right ones.
    extensions.push_back(info.mid_extension);
    extensions.push_back(info.rid_extension);
    media_desc->ClearRtpHeaderExtensions();
    media_desc->set_rtp_header_extensions(extensions);

    // Restore SimulcastDescription. It should correspond to one from offer,
    // but it have to have receive layers instead of send. So we need to put
    // send layers from offer to receive layers in answer.
    cricket::SimulcastDescription simulcast_description;
    for (auto layer : info.simulcast_description.send_layers()) {
      simulcast_description.receive_layers().AddLayerWithAlternatives(layer);
    }
    media_desc->set_simulcast_description(simulcast_description);

    // Add simulcast media section.
    desc->AddContent(info.mid, info.media_protocol_type, media_desc);
  }

  // Now we need to add bundle line to have all media bundled together.
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  for (auto& content : desc->contents()) {
    bundle_group.AddContentName(content.mid());
  }
  if (desc->HasGroup(cricket::GROUP_TYPE_BUNDLE)) {
    desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  }
  desc->AddGroup(bundle_group);

  // Fix transport_infos: it have to have single info for simulcast section.
  std::vector<cricket::TransportInfo> transport_infos = desc->transport_infos();
  std::map<std::string, cricket::TransportDescription>
      mid_to_transport_description;
  for (auto info_it = transport_infos.begin();
       info_it != transport_infos.end();) {
    auto it = context.simulcast_infos_by_rid.find(info_it->content_name);
    if (it != context.simulcast_infos_by_rid.end()) {
      // This transport info correspond to some extra added media section.
      mid_to_transport_description.insert(
          {it->second.mid, info_it->description});
      transport_infos.erase(info_it);
    } else {
      ++info_it;
    }
  }
  for (auto& info : context.simulcast_infos) {
    transport_infos.emplace_back(info.mid,
                                 mid_to_transport_description.at(info.mid));
  }
  desc->set_transport_infos(transport_infos);

  auto patched_answer =
      absl::make_unique<JsepSessionDescription>(SdpType::kAnswer);
  patched_answer->Initialize(std::move(desc), answer->session_id(),
                             answer->session_version());
  return patched_answer;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
