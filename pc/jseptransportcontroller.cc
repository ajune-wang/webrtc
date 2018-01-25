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
  MSG_RECEIVING,
  MSG_ICEGATHERINGSTATE,
  MSG_CANDIDATESGATHERED,
};

struct CandidatesData : public rtc::MessageData {
  CandidatesData(const std::string& transport_name,
                 const cricket::Candidates& candidates)
      : transport_name(transport_name), candidates(candidates) {}

  std::string transport_name;
  cricket::Candidates candidates;
};

bool VerifyCandidate(const cricket::Candidate& cand, std::string* error) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAnyIP()) {
    *error = "candidate has address of zero";
    return false;
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (cand.protocol() == cricket::TCP_PROTOCOL_NAME &&
      (cand.tcptype() == cricket::TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return true;
  }
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      *error = "candidate has port below 1024, but not 80 or 443";
      return false;
    }

    if (cand.address().IsPrivateIP()) {
      *error = "candidate has port of 80 or 443 with private IP address";
      return false;
    }
  }

  return true;
}

bool VerifyCandidates(const cricket::Candidates& candidates,
                      std::string* error) {
  for (const cricket::Candidate& candidate : candidates) {
    if (!VerifyCandidate(candidate, error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace webrtc {

JsepTransportController::JsepTransportController(rtc::Thread* signaling_thread,
                                                 rtc::Thread* network_thread,
                                                 PortAllocator* port_allocator,
                                                 Config config)
    : signaling_thread_(signaling_thread),
      network_thread_(network_thread),
      port_allocator_(port_allocator),
      config_(config),
      redetermine_role_on_ice_restart_(config.redetermine_role_on_ice_restart),
      crypto_options_(config.crypto_options),
      ssl_max_version_(config.ssl_max_version) {}

JsepTransportController::~JsepTransportController() {
  // Channel destructors may try to send packets, so this needs to happen on
  // the network thread.
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::DestroyAllTransports_n, this));
}

RTCError JsepTransportController::ApplyDescription(
    cricket::ContentSource source,
    SdpType type,
    const cricket::SessionDescription* description) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(RTC_FROM_HERE, [&] {
      return ApplyDescription(source, type, description);
    });
  }

  RTC_DCHECK(description);
  source == cricket::CS_LOCAL ? local_desc_ = description
                              : remote_desc_ = description;

  std::vector<int> merged_encrypted_extension_ids;
  if (ShouldEnableBundle(source, type, description)) {
    bundle_group_ = description->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    bundled_mid_ = *(bundle_group_->FirstContentName());
    merged_encrypted_extension_ids =
        MergeEncryptedHeaderExtensionIdsForBundle(description);
  }

  for (const ContentInfo& content_info : description->contents()) {
    if (content_info.rejected ||
        (bundle_group_ && content_info.name != bundled_mid_ &&
         bundle_group_->HasContentName(content_info.name))) {
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

    if (content_info.rejected) {
      if (content_info.type == cricket::MediaProtocolType::kRtp) {
        SignalRtpTransportChanged(content_info.name, nullptr);
      } else {
        SignalDtlsTransportChanged(content_info.name, nullptr);
      }
      MaybeDestroyJsepTransport(content_info.name);
      continue;
    }

    if (bundle_group_ && content_info.name != bundled_mid_ &&
        bundle_group_->HasContentName(content_info.name)) {
      if (content_info.type == cricket::MediaProtocolType::kRtp) {
        auto rtp_transport = transports_[bundled_mid_]->GetRtpTransport();
        SignalRtpTransportChanged(content_info.name, rtp_transport);
      } else {
        auto dtls_transport = transports_[bundled_mid_]->GetRtpDtlsTransport();
        SignalDtlsTransportChanged(content_info.name, dtls_transport);
      }
      MaybeDestroyJsepTransport(content_info.name);
      continue;
    }

    if (bundle_group_ && content_info.name == bundled_mid_) {
      extension_ids = merged_encrypted_extension_ids;
    } else {
      extension_ids = GetEncryptedHeaderExtensionIds(content_info);
    }

    bool success;
    std::string error;
    if (source == cricket::CS_LOCAL) {
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

RtpTransportInternal* JsepTransportController::GetRtpTransport(
    const std::string& mid) const {
  auto jsep_transport = GetJsepTransport(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->GetRtpTransport();
}

cricket::DtlsTransportInternal* JsepTransportController::GetDtlsTransport(
    const std::string& mid,
    int component) const {
  auto jsep_transport = GetJsepTransport(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return component == cricket::ICE_CANDIDATE_COMPONENT_RTP
             ? jsep_transport->GetRtpDtlsTransport()
             : jsep_transport->GetRtcpDtlsTransport();
}

void JsepTransportController::SetIceConfig(const cricket::IceConfig& config) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::SetIceConfig_n, this, config));
}

void JsepTransportController::SetNeedsIceRestartFlag() {
  for (auto& kv : transports_) {
    kv.second->SetNeedsIceRestartFlag();
  }
}

bool JsepTransportController::NeedsIceRestart(
    const std::string& transport_name) const {
  const JsepTransport2* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->NeedsIceRestart();
}

bool JsepTransportController::GetSslRole(const std::string& transport_name,
                                         rtc::SSLRole* role) const {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::GetSslRole_n, this,
                               transport_name, role));
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

bool JsepTransportController::AddRemoteCandidates(
    const std::string& transport_name,
    const Candidates& candidates,
    std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&JsepTransportController::AddRemoteCandidates_n,
                               this, transport_name, candidates, err));
}

bool JsepTransportController::RemoveRemoteCandidates(
    const Candidates& candidates,
    std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::RemoveRemoteCandidates_n, this,
                candidates, err));
}

bool JsepTransportController::ReadyForRemoteCandidates(
    const std::string& transport_name) const {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&JsepTransportController::ReadyForRemoteCandidates_n, this,
                transport_name));
}

bool JsepTransportController::GetStats(const std::string& transport_name,
                                       TransportStats* stats) {
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

std::unique_ptr<DtlsTransportInternal>
JsepTransportController::CreateDtlsTransport(const std::string& transport_name,
                                             int component) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // TODO(zhihuang): Add external transport_factory.
  std::unique_ptr<IceTransportInternal> ice =
      rtc::MakeUnique<cricket::P2PTransportChannel>(transport_name, component,
                                                    port_allocator_);
  std::unique_ptr<DtlsTransportInternal> dtls =
      rtc::MakeUnique<cricket::DtlsTransport>(std::move(ice), crypto_options_);
  dtls->SetSslMaxProtocolVersion(ssl_max_version_);
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
    bool rtcp_mux_enabled,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // TODO(zhihuang): Add support of unencrypted RTP for testing.
  return nullptr;
}

std::unique_ptr<webrtc::SrtpTransport>
JsepTransportController::CreateSdesTransport(
    const std::string& transport_name,
    bool rtcp_mux_enabled,
    rtc::PacketTransportInternal* rtp_packet_transport,
    rtc::PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  auto srtp_transport =
      rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);
  RTC_DCHECK(rtp_packet_transport);
  srtp_transport->SetRtpPacketTransport(rtp_packet_transport);
  if (!rtcp_mux_enabled) {
    RTC_DCHECK(rtcp_packet_transport);
    srtp_transport->SetRtcpPacketTransport(rtp_packet_transport);
  }
#if defined(ENABLE_EXTERNAL_AUTH)
  srtp_transport->EnableExternalAuth();
#endif
  return srtp_transport;
}

std::unique_ptr<webrtc::DtlsSrtpTransport>
JsepTransportController::CreateDtlsSrtpTransport(
    const std::string& transport_name,
    bool rtcp_mux_enabled,
    cricket::DtlsTransportInternal* rtp_dtls_transport,
    cricket::DtlsTransportInternal* rtcp_dtls_transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  auto srtp_transport =
      rtc::MakeUnique<webrtc::SrtpTransport>(rtcp_mux_enabled);

#if defined(ENABLE_EXTERNAL_AUTH)
  srtp_transport->EnableExternalAuth();
#endif

  auto dtls_srtp_transport =
      rtc::MakeUnique<webrtc::DtlsSrtpTransport>(std::move(srtp_transport));

  RTC_DCHECK((rtcp_mux_enabled && !rtcp_dtls_transport) ||
             (!rtcp_mux_enabled && rtcp_dtls_transport));
  dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport,
                                         rtcp_dtls_transport);
  return dtls_srtp_transport;
}

std::vector<DtlsTransportInternal*>
JsepTransportController::GetDtlsTransports() {
  std::vector<DtlsTransportInternal*> dtls_transports;
  for (auto it = transports_.begin(); it != transports_.end(); ++it) {
    auto jsep_transport = it->second.get();
    RTC_DCHECK(jsep_transport);
    if (jsep_transport->GetRtpDtlsTransport()) {
      dtls_transports.push_back(jsep_transport->GetRtpDtlsTransport());
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
      rtc::TypedMessageData<IceConnectionState>* data =
          static_cast<rtc::TypedMessageData<IceConnectionState>*>(pmsg->pdata);
      SignalConnectionState(data->data());
      delete data;
      break;
    }
    case MSG_RECEIVING: {
      rtc::TypedMessageData<bool>* data =
          static_cast<rtc::TypedMessageData<bool>*>(pmsg->pdata);
      SignalReceiving(data->data());
      delete data;
      break;
    }
    case MSG_ICEGATHERINGSTATE: {
      rtc::TypedMessageData<IceGatheringState>* data =
          static_cast<rtc::TypedMessageData<IceGatheringState>*>(pmsg->pdata);
      SignalGatheringState(data->data());
      delete data;
      break;
    }
    case MSG_CANDIDATESGATHERED: {
      CandidatesData* data = static_cast<CandidatesData*>(pmsg->pdata);
      SignalCandidatesGathered(data->transport_name, data->candidates);
      delete data;
      break;
    }
    default:
      RTC_NOTREACHED();
  }
}

bool JsepTransportController::ShouldEnableBundle(
    cricket::ContentSource source,
    SdpType type,
    const cricket::SessionDescription* description) {
  if (config_.max_bundle_required) {
    return true;
  }

  if (type == SdpType::kAnswer) {
    RTC_DCHECK(local_desc_ && remote_desc_);
    const cricket::ContentGroup* local_bundle =
        local_desc_->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    const cricket::ContentGroup* remote_bundle =
        remote_desc_->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
    return local_bundle && remote_bundle;
  }

  return false;
}

std::vector<int> JsepTransportController::GetEncryptedHeaderExtensionIds(
    const ContentInfo& content_info) {
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
  for (const ContentInfo& content_info : description->contents()) {
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

const JsepTransport2* JsepTransportController::GetJsepTransport(
    const std::string& transport_name) const {
  auto target_name = transport_name;
  if (bundle_group_ && bundle_group_->HasContentName(transport_name)) {
    target_name = bundled_mid_;
  }
  auto it = transports_.find(target_name);
  return (it == transports_.end()) ? nullptr : it->second.get();
}

JsepTransport2* JsepTransportController::GetJsepTransport(
    const std::string& transport_name) {
  auto target_name = transport_name;
  if (bundle_group_ && bundle_group_->HasContentName(transport_name)) {
    target_name = bundled_mid_;
  }
  auto it = transports_.find(target_name);
  return (it == transports_.end()) ? nullptr : it->second.get();
}

void JsepTransportController::MaybeCreateJsepTransport(
    const std::string& mid,
    const ContentInfo& content_info) {
  RTC_DCHECK(network_thread_->IsCurrent());

  JsepTransport2* transport = GetJsepTransport(mid);
  if (transport) {
    return;
  }

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);
  bool rtcp_mux_enabled = content_desc->rtcp_mux();
  std::unique_ptr<cricket::DtlsTransportInternal> rtp_dtls_transport =
      CreateDtlsTransport(mid, cricket::ICE_CANDIDATE_COMPONENT_RTP);
  std::unique_ptr<cricket::DtlsTransportInternal> rtcp_dtls_transport;
  if (!rtcp_mux_enabled) {
    rtcp_dtls_transport =
        CreateDtlsTransport(mid, cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }

  std::unique_ptr<RtpTransportInternal> rtp_transport;
  cricket::SrtpMode srtp_mode;
  if (config_.disable_encryption) {
    srtp_mode = cricket::SrtpMode::kUnencrypted;
    rtp_transport = CreateUnencryptedRtpTransport(mid, rtcp_mux_enabled,
                                                  rtp_dtls_transport.get(),
                                                  rtcp_dtls_transport.get());
  } else if (!content_desc->cryptos().empty()) {
    srtp_mode = cricket::SrtpMode::kSdes;
    rtp_transport =
        CreateSdesTransport(mid, rtcp_mux_enabled, rtp_dtls_transport.get(),
                            rtcp_dtls_transport.get());
  } else {
    srtp_mode = cricket::SrtpMode::kDtlsSrtp;
    rtp_transport =
        CreateDtlsSrtpTransport(mid, rtcp_mux_enabled, rtp_dtls_transport.get(),
                                rtcp_dtls_transport.get());
  }

  std::unique_ptr<JsepTransport2> jsep_transport =
      rtc::MakeUnique<JsepTransport2>(
          mid, certificate_, srtp_mode, std::move(rtp_transport),
          std::move(rtp_dtls_transport), std::move(rtcp_dtls_transport));
  jsep_transport->SignalRtcpMuxFullyActive.connect(
      this, &JsepTransportController::UpdateAggregateStates_n);
  transports_[mid] = std::move(jsep_transport);
  UpdateAggregateStates_n();
}

void JsepTransportController::MaybeDestroyJsepTransport(
    const std::string& mid) {
  transports_.erase(mid);
  UpdateAggregateStates_n();
}

void JsepTransportController::DestroyAllTransports_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  transports_.clear();
}

void JsepTransportController::SetIceConfig_n(const cricket::IceConfig& config) {
  RTC_DCHECK(network_thread_->IsCurrent());

  ice_config_ = config;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetIceConfig(ice_config_);
  }
}

void JsepTransportController::SetIceRole_n(IceRole ice_role) {
  RTC_DCHECK(network_thread_->IsCurrent());

  ice_role_ = ice_role;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetIceRole(ice_role_);
  }
}

bool JsepTransportController::GetSslRole_n(const std::string& transport_name,
                                           rtc::SSLRole* role) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const JsepTransport2* t = GetJsepTransport(transport_name);
  if (!t) {
    return false;
  }
  rtc::Optional<rtc::SSLRole> current_role = t->GetSslRole();
  if (!current_role) {
    return false;
  }
  *role = *current_role;
  return true;
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
  for (auto& kv : transports_) {
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

  const JsepTransport2* t = GetJsepTransport(transport_name);
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
  auto dtls =
      GetDtlsTransport(transport_name, cricket::ICE_CANDIDATE_COMPONENT_RTP);
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
  JsepTransport2* transport = GetJsepTransport(mid);
  RTC_DCHECK(transport);

  auto tdesc = transport_info.description;
  // The initial offer side may use ICE Lite, in which case, per RFC5245
  // Section 5.1.1, the answer side should take the controlling role if it is
  // in the full ICE mode.
  //
  // When both sides use ICE Lite, the initial offer side must take the
  // controlling role, and this is the default logic implemented in
  // SetLocalDescription in PeerConnection.
  if (transport->remote_description() &&
      transport->remote_description()->ice_mode == ICEMODE_LITE &&
      ice_role_ == ICEROLE_CONTROLLED && tdesc.ice_mode == ICEMODE_FULL) {
    SetIceRole_n(ICEROLE_CONTROLLING);
  }

  // Older versions of Chrome expect the ICE role to be re-determined when an
  // ICE restart occurs, and also don't perform conflict resolution correctly,
  // so for now we can't safely stop doing this, unless the application opts
  // in by setting |redetermine_role_on_ice_restart_| to false. See:
  // https://bugs.chromium.org/p/chromium/issues/detail?id=628676
  // TODO(deadbeef): Remove this when these old versions of Chrome reach a low
  // enough population.
  if (redetermine_role_on_ice_restart_ && transport->local_description() &&
      cricket::IceCredentialsChanged(transport->local_description()->ice_ufrag,
                                     transport->local_description()->ice_pwd,
                                     tdesc.ice_ufrag, tdesc.ice_pwd) &&
      // Don't change the ICE role if the remote endpoint is ICE lite; we
      // should always be controlling in that case.
      (!transport->remote_description() ||
       transport->remote_description()->ice_mode != ICEMODE_LITE)) {
    IceRole new_ice_role =
        (type == SdpType::kOffer) ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;
    SetIceRole_n(new_ice_role);
  }

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);
  RTC_DCHECK(content_desc);
  return transport->SetLocalTransportDescription(
      transport_info.description, content_desc->rtcp_mux(),
      content_desc->cryptos(), encrypted_extension_ids, type, error);
}

bool JsepTransportController::SetRemoteTransportDescription_n(
    const std::string& mid,
    const cricket::ContentInfo& content_info,
    const cricket::TransportInfo& transport_info,
    SdpType type,
    const std::vector<int> encrypted_extension_ids,
    std::string* error) {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto tdesc = transport_info.description;
  // If our role is ICEROLE_CONTROLLED and the remote endpoint supports only
  // ice_lite, this local endpoint should take the CONTROLLING role.
  // TODO(deadbeef): This is a session-level attribute, so it really shouldn't
  // be in a TransportDescription in the first place...
  if (ice_role_ == ICEROLE_CONTROLLED && tdesc.ice_mode == ICEMODE_LITE) {
    SetIceRole_n(ICEROLE_CONTROLLING);
  }

  JsepTransport2* transport = GetJsepTransport(mid);
  RTC_DCHECK(transport);

  // If we use ICE Lite and the remote endpoint uses the full implementation of
  // ICE, the local endpoint must take the controlled role, and the other side
  // must be the controlling role.
  if (transport->local_description() &&
      transport->local_description()->ice_mode == ICEMODE_LITE &&
      ice_role_ == ICEROLE_CONTROLLING && tdesc.ice_mode == ICEMODE_FULL) {
    SetIceRole_n(ICEROLE_CONTROLLED);
  }

  const cricket::MediaContentDescription* content_desc =
      static_cast<const cricket::MediaContentDescription*>(
          content_info.description);
  RTC_DCHECK(content_desc);
  return transport->SetRemoteTransportDescription(
      transport_info.description, content_desc->rtcp_mux(),
      content_desc->cryptos(), encrypted_extension_ids, type, error);
}

void JsepTransportController::MaybeStartGathering_n() {
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->MaybeStartGathering();
  }
}

bool JsepTransportController::AddRemoteCandidates_n(
    const std::string& transport_name,
    const Candidates& candidates,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Verify each candidate before passing down to the transport layer.
  if (!VerifyCandidates(candidates, err)) {
    return false;
  }

  JsepTransport2* jsep_transport = GetJsepTransport(transport_name);
  if (!jsep_transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    return true;
  }

  for (const Candidate& candidate : candidates) {
    auto dtls = candidate.component() == cricket::ICE_CANDIDATE_COMPONENT_RTP
                    ? jsep_transport->GetRtpDtlsTransport()
                    : jsep_transport->GetRtcpDtlsTransport();
    if (!dtls) {
      *err = "Candidate has an unknown component: " + candidate.ToString() +
             " for content: " + transport_name;
      return false;
    }
    dtls->ice_transport()->AddRemoteCandidate(candidate);
  }
  return true;
}

bool JsepTransportController::RemoveRemoteCandidates_n(
    const Candidates& candidates,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Verify each candidate before passing down to the transport layer.
  if (!VerifyCandidates(candidates, err)) {
    return false;
  }

  std::map<std::string, Candidates> candidates_by_transport_name;
  for (const Candidate& cand : candidates) {
    if (!cand.transport_name().empty()) {
      candidates_by_transport_name[cand.transport_name()].push_back(cand);
    } else {
      RTC_LOG(LS_ERROR) << "Not removing candidate because it does not have a "
                           "transport name set: "
                        << cand.ToString();
    }
  }

  bool result = true;
  for (const auto& kv : candidates_by_transport_name) {
    const std::string& transport_name = kv.first;
    const Candidates& candidates = kv.second;
    JsepTransport2* jsep_transport = GetJsepTransport(transport_name);
    if (!jsep_transport) {
      // If we didn't find a transport, that's not an error;
      // it could have been deleted as a result of bundling.
      continue;
    }
    for (const Candidate& candidate : candidates) {
      auto dtls = candidate.component() == cricket::ICE_CANDIDATE_COMPONENT_RTP
                      ? jsep_transport->GetRtpDtlsTransport()
                      : jsep_transport->GetRtcpDtlsTransport();
      if (dtls) {
        dtls->ice_transport()->RemoveRemoteCandidate(candidate);
      }
    }
  }
  return result;
}

bool JsepTransportController::ReadyForRemoteCandidates_n(
    const std::string& transport_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const JsepTransport2* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->ready_for_remote_candidates();
}

bool JsepTransportController::GetStats_n(const std::string& transport_name,
                                         TransportStats* stats) {
  RTC_DCHECK(network_thread_->IsCurrent());

  JsepTransport2* transport = GetJsepTransport(transport_name);
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
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void JsepTransportController::OnChannelCandidateGathered_n(
    IceTransportInternal* channel,
    const Candidate& candidate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // We should never signal peer-reflexive candidates.
  if (candidate.type() == PRFLX_PORT_TYPE) {
    RTC_NOTREACHED();
    return;
  }
  std::vector<Candidate> candidates;
  candidates.push_back(candidate);
  CandidatesData* data =
      new CandidatesData(channel->transport_name(), candidates);
  signaling_thread_->Post(RTC_FROM_HERE, this, MSG_CANDIDATESGATHERED, data);
}

void JsepTransportController::OnChannelCandidatesRemoved_n(
    IceTransportInternal* channel,
    const Candidates& candidates) {
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&JsepTransportController::OnChannelCandidatesRemoved, this,
                candidates));
}

void JsepTransportController::OnChannelCandidatesRemoved(
    const Candidates& candidates) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  SignalCandidatesRemoved(candidates);
}

void JsepTransportController::OnChannelRoleConflict_n(
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Note: since the role conflict is handled entirely on the network thread,
  // we don't need to worry about role conflicts occurring on two ports at once.
  // The first one encountered should immediately reverse the role.
  IceRole reversed_role = (ice_role_ == ICEROLE_CONTROLLING)
                              ? ICEROLE_CONTROLLED
                              : ICEROLE_CONTROLLING;
  RTC_LOG(LS_INFO) << "Got role conflict; switching to "
                   << (reversed_role == ICEROLE_CONTROLLING ? "controlling"
                                                            : "controlled")
                   << " role.";
  SetIceRole_n(reversed_role);
}

void JsepTransportController::OnChannelStateChanged_n(
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
                   << channel->component()
                   << " state changed. Check if state is complete.";
  UpdateAggregateStates_n();
}

void JsepTransportController::UpdateAggregateStates_n() {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto dtls_transport = GetDtlsTransports();
  IceConnectionState new_connection_state = kIceConnectionConnecting;
  IceGatheringState new_gathering_state = kIceGatheringNew;
  bool any_receiving = false;
  bool any_failed = false;
  bool all_connected = !dtls_transport.empty();
  bool all_completed = !dtls_transport.empty();
  bool any_gathering = false;
  bool all_done_gathering = !dtls_transport.empty();
  for (const auto& dtls : dtls_transport) {
    any_receiving = any_receiving || dtls->receiving();
    any_failed = any_failed || dtls->ice_transport()->GetState() ==
                                   IceTransportState::STATE_FAILED;
    all_connected = all_connected && dtls->writable();
    all_completed =
        all_completed && dtls->writable() &&
        dtls->ice_transport()->GetState() ==
            IceTransportState::STATE_COMPLETED &&
        dtls->ice_transport()->GetIceRole() == ICEROLE_CONTROLLING &&
        dtls->ice_transport()->gathering_state() == kIceGatheringComplete;
    any_gathering = any_gathering || dtls->ice_transport()->gathering_state() !=
                                         kIceGatheringNew;
    all_done_gathering =
        all_done_gathering &&
        dtls->ice_transport()->gathering_state() == kIceGatheringComplete;
  }
  if (any_failed) {
    new_connection_state = kIceConnectionFailed;
  } else if (all_completed) {
    new_connection_state = kIceConnectionCompleted;
  } else if (all_connected) {
    new_connection_state = kIceConnectionConnected;
  }
  if (connection_state_ != new_connection_state) {
    connection_state_ = new_connection_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICECONNECTIONSTATE,
        new rtc::TypedMessageData<IceConnectionState>(new_connection_state));
  }

  if (receiving_ != any_receiving) {
    receiving_ = any_receiving;
    signaling_thread_->Post(RTC_FROM_HERE, this, MSG_RECEIVING,
                            new rtc::TypedMessageData<bool>(any_receiving));
  }

  if (all_done_gathering) {
    new_gathering_state = kIceGatheringComplete;
  } else if (any_gathering) {
    new_gathering_state = kIceGatheringGathering;
  }
  if (gathering_state_ != new_gathering_state) {
    gathering_state_ = new_gathering_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICEGATHERINGSTATE,
        new rtc::TypedMessageData<IceGatheringState>(new_gathering_state));
  }
}

void JsepTransportController::OnDtlsHandshakeError(
    rtc::SSLHandshakeError error) {
  SignalDtlsHandshakeError(error);
}

}  // namespace webrtc
