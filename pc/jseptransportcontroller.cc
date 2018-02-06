/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jseptransportcontroller.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "p2p/base/port.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread.h"

using webrtc::SdpType;

namespace {

enum {
  MSG_ICECONNECTIONSTATE,
  MSG_ICERECEIVING,
  MSG_ICEGATHERINGSTATE,
  MSG_ICECANDIDATESGATHERED,
};

struct CandidatesData : public rtc::MessageData {
  CandidatesData(const std::string& transport_name,
                 const cricket::Candidates& candidates)
      : transport_name(transport_name), candidates(candidates) {}

  std::string transport_name;
  cricket::Candidates candidates;
};

webrtc::RTCError VerifyCandidate(const cricket::Candidate& cand) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAnyIP()) {
    return webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER,
                            "candidate has address of zero");
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (cand.protocol() == cricket::TCP_PROTOCOL_NAME &&
      (cand.tcptype() == cricket::TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return webrtc::RTCError::OK();
  }
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      return webrtc::RTCError(
          webrtc::RTCErrorType::INVALID_PARAMETER,
          "candidate has port below 1024, but not 80 or 443");
    }

    if (cand.address().IsPrivateIP()) {
      return webrtc::RTCError(
          webrtc::RTCErrorType::INVALID_PARAMETER,
          "candidate has port of 80 or 443 with private IP address");
    }
  }

  return webrtc::RTCError::OK();
}

webrtc::RTCError VerifyCandidates(const cricket::Candidates& candidates) {
  for (const cricket::Candidate& candidate : candidates) {
    webrtc::RTCError error = VerifyCandidate(candidate);
    if (!error.ok()) {
      return error;
    }
  }
  return webrtc::RTCError::OK();
}

}  // namespace

namespace webrtc {

JsepTransportController::JsepTransportController(
    rtc::Thread* signaling_thread,
    rtc::Thread* network_thread,
    cricket::PortAllocator* port_allocator,
    Config config)
    : signaling_thread_(signaling_thread),
      network_thread_(network_thread),
      port_allocator_(port_allocator),
      config_(config) {}

JsepTransportController::~JsepTransportController() {
  // Channel destructors may try to send packets, so this needs to happen on
  // the network thread.
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::DestroyAllJsepTransports_n, this));
}

RTCError JsepTransportController::SetLocalDescription(
    SdpType type,
    const cricket::SessionDescription* description) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(
        RTC_FROM_HERE, [=] { return SetLocalDescription(type, description); });
  }

  if (!initial_offerer_.has_value()) {
    initial_offerer_.emplace(type == SdpType::kOffer);
    if (*initial_offerer_) {
      SetIceRole_n(cricket::ICEROLE_CONTROLLING);
    } else {
      SetIceRole_n(cricket::ICEROLE_CONTROLLED);
    }
  }
  return ApplyDescription_n(/*local=*/true, type, description);
}

RTCError JsepTransportController::SetRemoteDescription(
    SdpType type,
    const cricket::SessionDescription* description) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(
        RTC_FROM_HERE, [=] { return SetRemoteDescription(type, description); });
  }

  return ApplyDescription_n(/*local=*/false, type, description);
}

RtpTransportInternal* JsepTransportController::GetRtpTransport(
    const std::string& mid) const {
  auto jsep_transport = GetJsepTransport(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->GetRtpTransport();
}

cricket::DtlsTransportInternal* JsepTransportController::GetDtlsTransport(
    const std::string& mid) const {
  auto jsep_transport = GetJsepTransport(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->GetDtlsTransport();
}

cricket::DtlsTransportInternal* JsepTransportController::GetRtcpDtlsTransport(
    const std::string& mid) const {
  auto jsep_transport = GetJsepTransport(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->GetRtcpDtlsTransport();
}

void JsepTransportController::SetIceConfig(const cricket::IceConfig& config) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::SetIceConfig_n, this, config));
}

void JsepTransportController::SetNeedsIceRestartFlag() {
  for (auto& kv : jsep_transports_by_mid_) {
    kv.second->SetNeedsIceRestartFlag();
  }
}

bool JsepTransportController::NeedsIceRestart(
    const std::string& transport_name) const {
  const cricket::JsepTransport2* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->NeedsIceRestart();
}

rtc::Optional<rtc::SSLRole> JsepTransportController::GetDtlsRole(
    const std::string& transport_name) const {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<rtc::Optional<rtc::SSLRole>>(
        RTC_FROM_HERE, [&] { return GetDtlsRole(transport_name); });
  }

  const cricket::JsepTransport2* t = GetJsepTransport(transport_name);
  if (!t) {
    return rtc::Optional<rtc::SSLRole>();
  }
  return t->GetSslRole();
}

bool JsepTransportController::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::SetLocalCertificate_n,
                               this, certificate));
}

bool JsepTransportController::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  if (network_thread_->IsCurrent()) {
    return GetLocalCertificate_n(transport_name, certificate);
  }
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::GetLocalCertificate_n,
                               this, transport_name, certificate));
}

std::unique_ptr<rtc::SSLCertificate>
JsepTransportController::GetRemoteSSLCertificate(
    const std::string& transport_name) const {
  if (network_thread_->IsCurrent()) {
    return GetRemoteSSLCertificate_n(transport_name);
  }
  return network_thread_->Invoke<std::unique_ptr<rtc::SSLCertificate>>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::GetRemoteSSLCertificate_n, this,
                transport_name));
}

void JsepTransportController::MaybeStartGathering() {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::MaybeStartGathering_n, this));
}

RTCError JsepTransportController::AddRemoteCandidates(
    const std::string& transport_name,
    const cricket::Candidates& candidates) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(RTC_FROM_HERE, [&] {
      return AddRemoteCandidates(transport_name, candidates);
    });
  }

  // Verify each candidate before passing down to the transport layer.
  RTCError error = VerifyCandidates(candidates);
  if (!error.ok()) {
    return error;
  }

  if (!ReadyForRemoteCandidates_n(transport_name)) {
    return RTCError(
        RTCErrorType::INVALID_STATE,
        transport_name + " is not ready to use the remote candidate.");
  }

  cricket::JsepTransport2* jsep_transport = GetJsepTransport(transport_name);
  if (!jsep_transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    return RTCError::OK();
  }

  for (const cricket::Candidate& candidate : candidates) {
    auto dtls = candidate.component() == cricket::ICE_CANDIDATE_COMPONENT_RTP
                    ? jsep_transport->GetDtlsTransport()
                    : jsep_transport->GetRtcpDtlsTransport();
    if (!dtls) {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "cricket::Candidate has an unknown component: " +
                          candidate.ToString() +
                          " for content: " + transport_name);
    }
    dtls->ice_transport()->AddRemoteCandidate(candidate);
  }
  return RTCError::OK();
}

RTCError JsepTransportController::RemoveRemoteCandidates(
    const cricket::Candidates& candidates) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(
        RTC_FROM_HERE, [&] { return RemoveRemoteCandidates(candidates); });
  }

  // Verify each candidate before passing down to the transport layer.
  RTCError error = VerifyCandidates(candidates);
  if (!error.ok()) {
    return error;
  }

  std::map<std::string, cricket::Candidates> candidates_by_transport_name;
  for (const cricket::Candidate& cand : candidates) {
    if (!cand.transport_name().empty()) {
      candidates_by_transport_name[cand.transport_name()].push_back(cand);
    } else {
      RTC_LOG(LS_ERROR) << "Not removing candidate because it does not have a "
                           "transport name set: "
                        << cand.ToString();
    }
  }

  for (const auto& kv : candidates_by_transport_name) {
    const std::string& transport_name = kv.first;
    const cricket::Candidates& candidates = kv.second;
    cricket::JsepTransport2* jsep_transport = GetJsepTransport(transport_name);
    if (!jsep_transport) {
      // If we didn't find a transport, that's not an error;
      // it could have been deleted as a result of bundling.
      continue;
    }
    for (const cricket::Candidate& candidate : candidates) {
      auto dtls = candidate.component() == cricket::ICE_CANDIDATE_COMPONENT_RTP
                      ? jsep_transport->GetDtlsTransport()
                      : jsep_transport->GetRtcpDtlsTransport();
      if (dtls) {
        dtls->ice_transport()->RemoveRemoteCandidate(candidate);
      }
    }
  }
  return RTCError::OK();
}

bool JsepTransportController::GetStats(const std::string& transport_name,
                                       cricket::TransportStats* stats) {
  if (network_thread_->IsCurrent()) {
    return GetStats_n(transport_name, stats);
  }
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::GetStats_n, this,
                               transport_name, stats));
}

void JsepTransportController::SetMetricsObserver(
    webrtc::MetricsObserverInterface* metrics_observer) {
  return network_thread_->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::SetMetricsObserver_n,
                               this, metrics_observer));
}

std::unique_ptr<cricket::DtlsTransportInternal>
JsepTransportController::CreateDtlsTransport(const std::string& transport_name,
                                             bool rtcp) {
  RTC_DCHECK(network_thread_->IsCurrent());
  int component = rtcp ? cricket::ICE_CANDIDATE_COMPONENT_RTCP
                       : cricket::ICE_CANDIDATE_COMPONENT_RTP;

  std::unique_ptr<cricket::DtlsTransportInternal> dtls;
  if (config_.external_transport_factory) {
    auto ice = config_.external_transport_factory->CreateIceTransport(
        transport_name, component);
    dtls = config_.external_transport_factory->CreateDtlsTransport(
        std::move(ice), config_.crypto_options);
  } else {
    auto ice = rtc::MakeUnique<cricket::P2PTransportChannel>(
        transport_name, component, port_allocator_);
    dtls = rtc::MakeUnique<cricket::DtlsTransport>(std::move(ice),
                                                   config_.crypto_options);
  }

  RTC_DCHECK(dtls);
  dtls->SetSslMaxProtocolVersion(config_.ssl_max_version);
  dtls->ice_transport()->SetMetricsObserver(metrics_observer_);
  dtls->ice_transport()->SetIceRole(ice_role_);
  dtls->ice_transport()->SetIceTiebreaker(ice_tiebreaker_);
  dtls->ice_transport()->SetIceConfig(ice_config_);
  if (certificate_) {
    bool set_cert_success = dtls->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }

  // Connect to signals offered by the channels. Currently, the DTLS channel
  // forwards signals from the ICE channel, so we only need to connect to the
  // DTLS channel. In the future this won't be the case.
  dtls->SignalWritableState.connect(
      this, &JsepTransportController::OnChannelWritableState_n);
  dtls->SignalReceivingState.connect(
      this, &JsepTransportController::OnChannelReceivingState_n);
  dtls->SignalDtlsHandshakeError.connect(
      this, &JsepTransportController::OnDtlsHandshakeError);
  dtls->ice_transport()->SignalGatheringState.connect(
      this, &JsepTransportController::OnChannelGatheringState_n);
  dtls->ice_transport()->SignalCandidateGathered.connect(
      this, &JsepTransportController::OnChannelCandidateGathered_n);
  dtls->ice_transport()->SignalCandidatesRemoved.connect(
      this, &JsepTransportController::OnChannelCandidatesRemoved_n);
  dtls->ice_transport()->SignalRoleConflict.connect(
      this, &JsepTransportController::OnChannelRoleConflict_n);
  dtls->ice_transport()->SignalStateChanged.connect(
      this, &JsepTransportController::OnChannelStateChanged_n);
  return dtls;
}

std::unique_ptr<webrtc::RtpTransport>
JsepTransportController::CreateUnencryptedRtpTransport(
    const std::string& transport_name,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // TODO(zhihuang): Add support of unencrypted RTP for testing.
  return nullptr;
}

std::unique_ptr<webrtc::SrtpTransport>
JsepTransportController::CreateSdesTransport(
    const std::string& transport_name,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  bool rtcp_mux_enabled = rtcp_packet_transport == nullptr;
  auto srtp_transport =
      rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);
  RTC_DCHECK(rtp_packet_transport);
  srtp_transport->SetRtpPacketTransport(rtp_packet_transport);
  if (rtcp_packet_transport) {
    srtp_transport->SetRtcpPacketTransport(rtp_packet_transport);
  }
  if (config_.enable_external_auth) {
    srtp_transport->EnableExternalAuth();
  }
  return srtp_transport;
}

std::unique_ptr<webrtc::DtlsSrtpTransport>
JsepTransportController::CreateDtlsSrtpTransport(
    const std::string& transport_name,
    cricket::DtlsTransportInternal* rtp_dtls_transport,
    cricket::DtlsTransportInternal* rtcp_dtls_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  bool rtcp_mux_enabled = rtcp_dtls_transport == nullptr;
  auto srtp_transport =
      rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);
  if (config_.enable_external_auth) {
    srtp_transport->EnableExternalAuth();
  }

  auto dtls_srtp_transport =
      rtc::MakeUnique<webrtc::DtlsSrtpTransport>(std::move(srtp_transport));

  RTC_DCHECK((rtcp_mux_enabled && !rtcp_dtls_transport) ||
             (!rtcp_mux_enabled && rtcp_dtls_transport));
  dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport,
                                         rtcp_dtls_transport);
  return dtls_srtp_transport;
}

std::vector<cricket::DtlsTransportInternal*>
JsepTransportController::GetDtlsTransports() {
  std::vector<cricket::DtlsTransportInternal*> dtls_transports;
  for (auto it = jsep_transports_by_mid_.begin();
       it != jsep_transports_by_mid_.end(); ++it) {
    auto jsep_transport = it->second.get();
    RTC_DCHECK(jsep_transport);
    if (jsep_transport->GetDtlsTransport()) {
      dtls_transports.push_back(jsep_transport->GetDtlsTransport());
    }

    if (jsep_transport->GetRtcpDtlsTransport()) {
      dtls_transports.push_back(jsep_transport->GetRtcpDtlsTransport());
    }
  }
  return dtls_transports;
}

void JsepTransportController::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  switch (pmsg->message_id) {
    case MSG_ICECONNECTIONSTATE: {
      rtc::TypedMessageData<cricket::IceConnectionState>* data =
          static_cast<rtc::TypedMessageData<cricket::IceConnectionState>*>(
              pmsg->pdata);
      SignalIceConnectionState(data->data());
      delete data;
      break;
    }
    case MSG_ICERECEIVING: {
      rtc::TypedMessageData<bool>* data =
          static_cast<rtc::TypedMessageData<bool>*>(pmsg->pdata);
      SignalIceReceiving(data->data());
      delete data;
      break;
    }
    case MSG_ICEGATHERINGSTATE: {
      rtc::TypedMessageData<cricket::IceGatheringState>* data =
          static_cast<rtc::TypedMessageData<cricket::IceGatheringState>*>(
              pmsg->pdata);
      SignalIceGatheringState(data->data());
      delete data;
      break;
    }
    case MSG_ICECANDIDATESGATHERED: {
      CandidatesData* data = static_cast<CandidatesData*>(pmsg->pdata);
      SignalIceCandidatesGathered(data->transport_name, data->candidates);
      delete data;
      break;
    }
    default:
      RTC_NOTREACHED();
  }
}

RTCError JsepTransportController::ApplyDescription_n(
    bool local,
    SdpType type,
    const cricket::SessionDescription* description) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_DCHECK(description);

  if (local) {
    local_desc_ = description;
  } else {
    remote_desc_ = description;
  }

  std::vector<int> merged_encrypted_extension_ids;
  if (ShouldEnableBundle(type, description)) {
    bundle_group_ = description->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    merged_encrypted_extension_ids =
        MergeEncryptedHeaderExtensionIdsForBundle(description);
  }

  for (const cricket::ContentInfo& content_info : description->contents()) {
    // Don't create transports for rejected m-lines and bundled m-lines."
    if (content_info.rejected ||
        (IsBundled(content_info.name) && content_info.name != *bundled_mid())) {
      continue;
    }
    MaybeCreateJsepTransport(content_info.name, content_info);
  }

  RTC_DCHECK(description->contents().size() ==
             description->transport_infos().size());
  for (size_t i = 0; i < description->contents().size(); ++i) {
    const cricket::ContentInfo& content_info = description->contents()[i];
    const cricket::TransportInfo& transport_info =
        description->transport_infos()[i];

    std::vector<int> extension_ids;

    // If the content is rejected or bundle is enabled, let the
    // BaseChannel/SctpTransport change the RtpTransport/DtlsTransport first,
    // then destroy the cricket::JsepTransport2.
    if (content_info.rejected) {
      if (content_info.type == cricket::MediaProtocolType::kRtp) {
        SignalRtpTransportChanged(content_info.name, nullptr);
      } else {
        SignalDtlsTransportChanged(content_info.name, nullptr);
      }
      MaybeDestroyJsepTransport(content_info.name);
      continue;
    }

    if (IsBundled(content_info.name) && content_info.name != *bundled_mid()) {
      if (content_info.type == cricket::MediaProtocolType::kRtp) {
        auto rtp_transport =
            jsep_transports_by_mid_[*bundled_mid()]->GetRtpTransport();
        SignalRtpTransportChanged(content_info.name, rtp_transport);
      } else {
        auto dtls_transport =
            jsep_transports_by_mid_[*bundled_mid()]->GetDtlsTransport();
        SignalDtlsTransportChanged(content_info.name, dtls_transport);
      }
      MaybeDestroyJsepTransport(content_info.name);
      continue;
    }

    if (bundle_group_ && content_info.name == *bundled_mid()) {
      extension_ids = merged_encrypted_extension_ids;
    } else {
      extension_ids = GetEncryptedHeaderExtensionIds(content_info);
    }

    bool success;
    std::string error;
    if (local) {
      success = SetLocalTransportDescription_n(content_info.name, content_info,
                                               transport_info, type,
                                               extension_ids, &error);
    } else {
      success = SetRemoteTransportDescription_n(content_info.name, content_info,
                                                transport_info, type,
                                                extension_ids, &error);
    }

    if (!success) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Failed to apply the description for " +
                               content_info.name + ": " + error);
    }
  }
  return RTCError::OK();
}

bool JsepTransportController::ShouldEnableBundle(
    SdpType type,
    const cricket::SessionDescription* description) {
  // If BUNDLE is enable in previous offer/answer negotiation, it would be
  // enabled in the subsequent offer/answer by default.
  if (bundle_group_) {
    return true;
  }

  if (config_.bundle_policy ==
      PeerConnectionInterface::kBundlePolicyMaxBundle) {
    return true;
  }

  if (type != SdpType::kAnswer) {
    return false;
  }

  RTC_DCHECK(local_desc_ && remote_desc_);
  const cricket::ContentGroup* local_bundle =
      local_desc_->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  const cricket::ContentGroup* remote_bundle =
      remote_desc_->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  return local_bundle && remote_bundle;
}

std::vector<int> JsepTransportController::GetEncryptedHeaderExtensionIds(
    const cricket::ContentInfo& content_info) {
  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);

  if (!config_.crypto_options.enable_encrypted_rtp_header_extensions) {
    return std::vector<int>();
  }

  std::vector<int> encrypted_header_extension_ids;
  for (auto extension : content_desc->rtp_header_extensions()) {
    if (!extension.encrypt) {
      continue;
    }
    auto it = std::find(encrypted_header_extension_ids.begin(),
                        encrypted_header_extension_ids.end(), extension.id);
    if (it == encrypted_header_extension_ids.end()) {
      encrypted_header_extension_ids.push_back(extension.id);
    }
  }
  return encrypted_header_extension_ids;
}

std::vector<int>
JsepTransportController::MergeEncryptedHeaderExtensionIdsForBundle(
    const cricket::SessionDescription* description) {
  RTC_DCHECK(description);
  RTC_DCHECK(bundle_group_);

  std::vector<int> merged_ids;
  // Union the encrypted header IDs in the group when bundle is enabled.
  for (const cricket::ContentInfo& content_info : description->contents()) {
    if (bundle_group_->HasContentName(content_info.name)) {
      std::vector<int> extension_ids =
          GetEncryptedHeaderExtensionIds(content_info);
      for (int id : extension_ids) {
        auto it = std::find(merged_ids.begin(), merged_ids.end(), id);
        if (it == merged_ids.end()) {
          merged_ids.push_back(id);
        }
      }
    }
  }
  return merged_ids;
}

const cricket::JsepTransport2* JsepTransportController::GetJsepTransport(
    const std::string& transport_name) const {
  auto target_name = transport_name;
  if (IsBundled(transport_name)) {
    target_name = *bundled_mid();
  }
  auto it = jsep_transports_by_mid_.find(target_name);
  return (it == jsep_transports_by_mid_.end()) ? nullptr : it->second.get();
}

cricket::JsepTransport2* JsepTransportController::GetJsepTransport(
    const std::string& transport_name) {
  auto target_name = transport_name;
  if (IsBundled(transport_name)) {
    target_name = *bundled_mid();
  }
  auto it = jsep_transports_by_mid_.find(target_name);
  return (it == jsep_transports_by_mid_.end()) ? nullptr : it->second.get();
}

void JsepTransportController::MaybeCreateJsepTransport(
    const std::string& mid,
    const cricket::ContentInfo& content_info) {
  RTC_DCHECK(network_thread_->IsCurrent());

  cricket::JsepTransport2* transport = GetJsepTransport(mid);
  if (transport) {
    return;
  }

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);
  bool rtcp_mux_enabled =
      content_desc->rtcp_mux() ||
      config_.rtcp_mux_policy == PeerConnectionInterface::kRtcpMuxPolicyRequire;
  std::unique_ptr<cricket::DtlsTransportInternal> rtp_dtls_transport =
      CreateDtlsTransport(mid, /*rtcp = */ false);
  std::unique_ptr<cricket::DtlsTransportInternal> rtcp_dtls_transport;
  if (!rtcp_mux_enabled) {
    rtcp_dtls_transport = CreateDtlsTransport(mid, /*rtcp = */ true);
  }

  std::unique_ptr<RtpTransportInternal> rtp_transport;
  cricket::SrtpMode srtp_mode;
  if (config_.disable_encryption) {
    srtp_mode = cricket::SrtpMode::kUnencrypted;
    rtp_transport = CreateUnencryptedRtpTransport(mid, rtp_dtls_transport.get(),
                                                  rtcp_dtls_transport.get());
  } else if (!content_desc->cryptos().empty()) {
    srtp_mode = cricket::SrtpMode::kSdes;
    rtp_transport = CreateSdesTransport(mid, rtp_dtls_transport.get(),
                                        rtcp_dtls_transport.get());
  } else {
    srtp_mode = cricket::SrtpMode::kDtlsSrtp;
    rtp_transport = CreateDtlsSrtpTransport(mid, rtp_dtls_transport.get(),
                                            rtcp_dtls_transport.get());
  }

  std::unique_ptr<cricket::JsepTransport2> jsep_transport =
      rtc::MakeUnique<cricket::JsepTransport2>(
          mid, certificate_, srtp_mode, std::move(rtp_transport),
          std::move(rtp_dtls_transport), std::move(rtcp_dtls_transport));
  jsep_transport->SignalRtcpMuxFullyActive.connect(
      this, &JsepTransportController::UpdateAggregateStates_n);
  jsep_transports_by_mid_[mid] = std::move(jsep_transport);
  UpdateAggregateStates_n();
}

void JsepTransportController::MaybeDestroyJsepTransport(
    const std::string& mid) {
  jsep_transports_by_mid_.erase(mid);
  UpdateAggregateStates_n();
}

void JsepTransportController::DestroyAllJsepTransports_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  jsep_transports_by_mid_.clear();
}

void JsepTransportController::SetIceConfig_n(const cricket::IceConfig& config) {
  RTC_DCHECK(network_thread_->IsCurrent());

  ice_config_ = config;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetIceConfig(ice_config_);
  }
}

void JsepTransportController::SetIceRole_n(cricket::IceRole ice_role) {
  RTC_DCHECK(network_thread_->IsCurrent());

  ice_role_ = ice_role;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetIceRole(ice_role_);
  }
}

bool JsepTransportController::SetLocalCertificate_n(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Can't change a certificate, or set a null certificate.
  if (certificate_ || !certificate) {
    return false;
  }
  certificate_ = certificate;

  // Set certificate for JsepTransport, which verifies it matches the
  // fingerprint in SDP, and DTLS transport.
  // Fallback from DTLS to SDES is not supported.
  for (auto& kv : jsep_transports_by_mid_) {
    kv.second->SetLocalCertificate(certificate_);
  }
  for (auto& dtls : GetDtlsTransports()) {
    bool set_cert_success = dtls->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }
  return true;
}

bool JsepTransportController::GetLocalCertificate_n(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const cricket::JsepTransport2* t = GetJsepTransport(transport_name);
  if (!t) {
    return false;
  }
  return t->GetLocalCertificate(certificate);
}

std::unique_ptr<rtc::SSLCertificate>
JsepTransportController::GetRemoteSSLCertificate_n(
    const std::string& transport_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Get the certificate from the RTP channel's DTLS handshake. Should be
  // identical to the RTCP channel's, since they were given the same remote
  // fingerprint.
  auto dtls = GetDtlsTransport(transport_name);
  if (!dtls) {
    return nullptr;
  }

  return dtls->GetRemoteSSLCertificate();
}

bool JsepTransportController::SetLocalTransportDescription_n(
    const std::string& mid,
    const cricket::ContentInfo& content_info,
    const cricket::TransportInfo& transport_info,
    SdpType type,
    const std::vector<int> encrypted_extension_ids,
    std::string* error) {
  cricket::JsepTransport2* transport = GetJsepTransport(mid);
  RTC_DCHECK(transport);

  SetIceRole_n(
      DetermineIceRole(transport, transport_info, type, /*local=*/true));

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);

  RTC_DCHECK(content_desc);
  bool rtcp_mux_enabled = content_info.type == cricket::MediaProtocolType::kSctp
                              ? true
                              : content_desc->rtcp_mux();

  return transport->SetLocalTransportDescription(
      transport_info.description, rtcp_mux_enabled, content_desc->cryptos(),
      encrypted_extension_ids, type, error);
}

bool JsepTransportController::SetRemoteTransportDescription_n(
    const std::string& mid,
    const cricket::ContentInfo& content_info,
    const cricket::TransportInfo& transport_info,
    SdpType type,
    const std::vector<int> encrypted_extension_ids,
    std::string* error) {
  RTC_DCHECK(network_thread_->IsCurrent());

  cricket::JsepTransport2* transport = GetJsepTransport(mid);
  RTC_DCHECK(transport);

  SetIceRole_n(
      DetermineIceRole(transport, transport_info, type, /*local=*/false));

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);

  RTC_DCHECK(content_desc);
  bool rtcp_mux_enabled = content_info.type == cricket::MediaProtocolType::kSctp
                              ? true
                              : content_desc->rtcp_mux();

  return transport->SetRemoteTransportDescription(
      transport_info.description, rtcp_mux_enabled, content_desc->cryptos(),
      encrypted_extension_ids, type, error);
}

cricket::IceRole JsepTransportController::DetermineIceRole(
    cricket::JsepTransport2* jsep_transport,
    const cricket::TransportInfo& transport_info,
    SdpType type,
    bool local) {
  cricket::IceRole ice_role = ice_role_;
  auto tdesc = transport_info.description;
  if (local) {
    // The initial offer side may use ICE Lite, in which case, per RFC5245
    // Section 5.1.1, the answer side should take the controlling role if it is
    // in the full ICE mode.
    //
    // When both sides use ICE Lite, the initial offer side must take the
    // controlling role, and this is the default logic implemented in
    // SetLocalDescription in JsepTransportController.
    if (jsep_transport->remote_description() &&
        jsep_transport->remote_description()->ice_mode ==
            cricket::ICEMODE_LITE &&
        ice_role_ == cricket::ICEROLE_CONTROLLED &&
        tdesc.ice_mode == cricket::ICEMODE_LITE) {
      ice_role = cricket::ICEROLE_CONTROLLING;
    }

    // Older versions of Chrome expect the ICE role to be re-determined when an
    // ICE restart occurs, and also don't perform conflict resolution correctly,
    // so for now we can't safely stop doing this, unless the application opts
    // in by setting |config_.redetermine_role_on_ice_restart_| to false. See:
    // https://bugs.chromium.org/p/chromium/issues/detail?id=628676
    // TODO(deadbeef): Remove this when these old versions of Chrome reach a low
    // enough population.
    if (config_.redetermine_role_on_ice_restart &&
        jsep_transport->local_description() &&
        cricket::IceCredentialsChanged(
            jsep_transport->local_description()->ice_ufrag,
            jsep_transport->local_description()->ice_pwd, tdesc.ice_ufrag,
            tdesc.ice_pwd) &&
        // Don't change the ICE role if the remote endpoint is ICE lite; we
        // should always be controlling in that case.
        (!jsep_transport->remote_description() ||
         jsep_transport->remote_description()->ice_mode !=
             cricket::ICEMODE_LITE)) {
      ice_role = (type == SdpType::kOffer) ? cricket::ICEROLE_CONTROLLING
                                           : cricket::ICEROLE_CONTROLLED;
    }
  } else {
    // If our role is cricket::ICEROLE_CONTROLLED and the remote endpoint
    // supports only ice_lite, this local endpoint should take the CONTROLLING
    // role.
    // TODO(deadbeef): This is a session-level attribute, so it really shouldn't
    // be in a TransportDescription in the first place...
    if (ice_role_ == cricket::ICEROLE_CONTROLLED &&
        tdesc.ice_mode == cricket::ICEMODE_LITE) {
      ice_role = cricket::ICEROLE_CONTROLLING;
    }

    // If we use ICE Lite and the remote endpoint uses the full implementation
    // of ICE, the local endpoint must take the controlled role, and the other
    // side must be the controlling role.
    if (jsep_transport->local_description() &&
        jsep_transport->local_description()->ice_mode ==
            cricket::ICEMODE_LITE &&
        ice_role_ == cricket::ICEROLE_CONTROLLING &&
        tdesc.ice_mode == cricket::ICEMODE_LITE) {
      ice_role = cricket::ICEROLE_CONTROLLED;
    }
  }

  return ice_role;
}

void JsepTransportController::MaybeStartGathering_n() {
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->MaybeStartGathering();
  }
}

bool JsepTransportController::ReadyForRemoteCandidates_n(
    const std::string& transport_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const cricket::JsepTransport2* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->ready_for_remote_candidates();
}

bool JsepTransportController::GetStats_n(const std::string& transport_name,
                                         cricket::TransportStats* stats) {
  RTC_DCHECK(network_thread_->IsCurrent());

  cricket::JsepTransport2* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->GetStats(stats);
}

void JsepTransportController::SetMetricsObserver_n(
    webrtc::MetricsObserverInterface* metrics_observer) {
  RTC_DCHECK(network_thread_->IsCurrent());
  metrics_observer_ = metrics_observer;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetMetricsObserver(metrics_observer);
  }
}

void JsepTransportController::OnChannelWritableState_n(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_LOG(LS_INFO) << " Transport " << transport->transport_name()
                   << " writability changed to " << transport->writable()
                   << ".";
  UpdateAggregateStates_n();
}

void JsepTransportController::OnChannelReceivingState_n(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void JsepTransportController::OnChannelGatheringState_n(
    cricket::IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void JsepTransportController::OnChannelCandidateGathered_n(
    cricket::IceTransportInternal* channel,
    const cricket::Candidate& candidate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // We should never signal peer-reflexive candidates.
  if (candidate.type() == cricket::PRFLX_PORT_TYPE) {
    RTC_NOTREACHED();
    return;
  }
  std::vector<cricket::Candidate> candidates;
  candidates.push_back(candidate);
  CandidatesData* data =
      new CandidatesData(channel->transport_name(), candidates);
  signaling_thread_->Post(RTC_FROM_HERE, this, MSG_ICECANDIDATESGATHERED, data);
}

void JsepTransportController::OnChannelCandidatesRemoved_n(
    cricket::IceTransportInternal* channel,
    const cricket::Candidates& candidates) {
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&JsepTransportController::OnChannelCandidatesRemoved, this,
                candidates));
}

void JsepTransportController::OnChannelCandidatesRemoved(
    const cricket::Candidates& candidates) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  SignalIceCandidatesRemoved(candidates);
}

void JsepTransportController::OnChannelRoleConflict_n(
    cricket::IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Note: since the role conflict is handled entirely on the network thread,
  // we don't need to worry about role conflicts occurring on two ports at
  // once. The first one encountered should immediately reverse the role.
  cricket::IceRole reversed_role = (ice_role_ == cricket::ICEROLE_CONTROLLING)
                                       ? cricket::ICEROLE_CONTROLLED
                                       : cricket::ICEROLE_CONTROLLING;
  RTC_LOG(LS_INFO) << "Got role conflict; switching to "
                   << (reversed_role == cricket::ICEROLE_CONTROLLING
                           ? "controlling"
                           : "controlled")
                   << " role.";
  SetIceRole_n(reversed_role);
}

void JsepTransportController::OnChannelStateChanged_n(
    cricket::IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
                   << channel->component()
                   << " state changed. Check if state is complete.";
  UpdateAggregateStates_n();
}

void JsepTransportController::UpdateAggregateStates_n() {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto dtls_transports = GetDtlsTransports();
  cricket::IceConnectionState new_connection_state =
      cricket::kIceConnectionConnecting;
  cricket::IceGatheringState new_gathering_state = cricket::kIceGatheringNew;
  bool any_receiving = false;
  bool any_failed = false;
  bool all_connected = !dtls_transports.empty();
  bool all_completed = !dtls_transports.empty();
  bool any_gathering = false;
  bool all_done_gathering = !dtls_transports.empty();
  for (const auto& dtls : dtls_transports) {
    any_receiving = any_receiving || dtls->receiving();
    any_failed = any_failed || dtls->ice_transport()->GetState() ==
                                   cricket::IceTransportState::STATE_FAILED;
    all_connected = all_connected && dtls->writable();
    all_completed =
        all_completed && dtls->writable() &&
        dtls->ice_transport()->GetState() ==
            cricket::IceTransportState::STATE_COMPLETED &&
        dtls->ice_transport()->GetIceRole() == cricket::ICEROLE_CONTROLLING &&
        dtls->ice_transport()->gathering_state() ==
            cricket::kIceGatheringComplete;
    any_gathering = any_gathering || dtls->ice_transport()->gathering_state() !=
                                         cricket::kIceGatheringNew;
    all_done_gathering =
        all_done_gathering && dtls->ice_transport()->gathering_state() ==
                                  cricket::kIceGatheringComplete;
  }
  if (any_failed) {
    new_connection_state = cricket::kIceConnectionFailed;
  } else if (all_completed) {
    new_connection_state = cricket::kIceConnectionCompleted;
  } else if (all_connected) {
    new_connection_state = cricket::kIceConnectionConnected;
  }
  if (ice_connection_state_ != new_connection_state) {
    ice_connection_state_ = new_connection_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICECONNECTIONSTATE,
        new rtc::TypedMessageData<cricket::IceConnectionState>(
            new_connection_state));
  }

  if (ice_receiving_ != any_receiving) {
    ice_receiving_ = any_receiving;
    signaling_thread_->Post(RTC_FROM_HERE, this, MSG_ICERECEIVING,
                            new rtc::TypedMessageData<bool>(any_receiving));
  }

  if (all_done_gathering) {
    new_gathering_state = cricket::kIceGatheringComplete;
  } else if (any_gathering) {
    new_gathering_state = cricket::kIceGatheringGathering;
  }
  if (ice_gathering_state_ != new_gathering_state) {
    ice_gathering_state_ = new_gathering_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICEGATHERINGSTATE,
        new rtc::TypedMessageData<cricket::IceGatheringState>(
            new_gathering_state));
  }
}

void JsepTransportController::OnDtlsHandshakeError(
    rtc::SSLHandshakeError error) {
  SignalDtlsHandshakeError(error);
}

}  // namespace webrtc
