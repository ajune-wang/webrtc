/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_SDP_SDP_CHANGER_H_
#define TEST_PC_E2E_SDP_SDP_CHANGER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/jsep.h"
#include "api/rtp_parameters.h"
#include "media/base/rid_description.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Creates list of capabilities, which can be set on RtpTransceiverInterface via
// RtpTransceiverInterface::SetCodecPreferences(...) to negotiate use of codec
// from list of |supported_codecs| with specified |codec_name| and parameters,
// which contains all of |codec_required_params|. If flags |ulpfec| or |flexfec|
// set to true corresponding FEC codec will be added. FEC and RTX codecs will be
// added after required codecs.
//
// All codecs will be added only if they exists in the list of
// |supported_codecs|. If multiple codecs from this list will have |codec_name|
// and |codec_required_params|, then all of them will be added to the output
// vector and they will be added in the same order, as they were in
// |supported_codecs|.
std::vector<RtpCodecCapability> FilterCodecCapabilities(
    absl::string_view codec_name,
    const std::map<std::string, std::string>& codec_required_params,
    bool ulpfec,
    bool flexfec,
    std::vector<RtpCodecCapability> supported_codecs);

// Contains information about simulcast section, that is required to perform
// modified offer/answer and ice candidates exchange.
struct SimulcastSectionInfo {
  SimulcastSectionInfo(const std::string& mid,
                       cricket::MediaProtocolType media_protocol_type,
                       const std::vector<cricket::RidDescription>& rids_desc)
      : mid(mid), media_protocol_type(media_protocol_type) {
    for (auto& rid : rids_desc) {
      rids.push_back(mid + "_" + rid.rid);
    }
  }

  const std::string mid;
  const cricket::MediaProtocolType media_protocol_type;
  std::vector<std::string> rids;
  cricket::SimulcastDescription simulcast_description;
  webrtc::RtpExtension mid_extension;
  webrtc::RtpExtension rid_extension;
  cricket::TransportDescription transport_description;
};

struct OfferAnswerExchangeSimulcastContext {
  void AddSimulcastInfo(const SimulcastSectionInfo& info) {
    simulcast_infos.push_back(info);
    RTC_CHECK(simulcast_infos_by_mid.insert({info.mid, simulcast_infos.back()})
                  .second);
    for (auto& rid : info.rids) {
      RTC_CHECK(
          simulcast_infos_by_rid.insert({rid, simulcast_infos.back()}).second);
    }
  }

  bool empty() const { return simulcast_infos.empty(); }

  // TODO(titovartem) optimize storage here.
  std::vector<SimulcastSectionInfo> simulcast_infos;
  std::map<std::string, SimulcastSectionInfo> simulcast_infos_by_mid;
  std::map<std::string, SimulcastSectionInfo> simulcast_infos_by_rid;

  std::vector<std::string> mids_order;
};

struct LocalAndRemoteOfferWithContext {
  LocalAndRemoteOfferWithContext(
      std::unique_ptr<SessionDescriptionInterface> offer_for_local,
      std::unique_ptr<SessionDescriptionInterface> offer_for_remote,
      OfferAnswerExchangeSimulcastContext context)
      : offer_for_local(std::move(offer_for_local)),
        offer_for_remote(std::move(offer_for_remote)),
        context(std::move(context)) {}

  // Offer to set as local description on local peer.
  std::unique_ptr<SessionDescriptionInterface> offer_for_local;
  // Offer to set as remote description on remote peer.
  std::unique_ptr<SessionDescriptionInterface> offer_for_remote;
  // Context required to correctly restore answer.
  OfferAnswerExchangeSimulcastContext context;
};

LocalAndRemoteOfferWithContext PatchOffer(
    std::unique_ptr<SessionDescriptionInterface> offer);

std::unique_ptr<SessionDescriptionInterface> PatchAnswer(
    const SessionDescriptionInterface& answer,
    const OfferAnswerExchangeSimulcastContext& context);

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_SDP_SDP_CHANGER_H_
