/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/fakejseptransport.h"

#include <memory>
#include <utility>  // for std::pair

#include "api/candidate.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/p2ptransportchannel.h"
#include "p2p/base/port.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

using webrtc::SdpType;

namespace cricket {

static bool VerifyIceParams(const TransportDescription& desc) {
  // For legacy protocols.
  if (desc.ice_ufrag.empty() && desc.ice_pwd.empty())
    return true;

  if (desc.ice_ufrag.length() < ICE_UFRAG_MIN_LENGTH ||
      desc.ice_ufrag.length() > ICE_UFRAG_MAX_LENGTH) {
    return false;
  }
  if (desc.ice_pwd.length() < ICE_PWD_MIN_LENGTH ||
      desc.ice_pwd.length() > ICE_PWD_MAX_LENGTH) {
    return false;
  }
  return true;
}

FakeJsepTransport::FakeJsepTransport(
    const std::string& mid,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
    : mid_(mid), certificate_(certificate) {}

FakeJsepTransport::~FakeJsepTransport() = default;

bool FakeJsepTransport::AddChannel(DtlsTransportInternal* dtls, int component) {
  if (channels_.find(component) != channels_.end()) {
    RTC_LOG(LS_ERROR) << "Adding channel for component " << component
                      << " twice.";
    return false;
  }
  channels_[component] = dtls;
  bool ret = true;
  std::string err;
  if (local_description_set_) {
    ret &= ApplyLocalTransportDescription(channels_[component], &err);
  }
  if (remote_description_set_) {
    ret &= ApplyRemoteTransportDescription(channels_[component], &err);
  }
  if (local_description_set_ && remote_description_set_) {
    ret &= ApplyNegotiatedTransportDescription(channels_[component], &err);
  }
  return ret;
}

bool FakeJsepTransport::GetLocalCertificate(
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  if (!certificate_) {
    return false;
  }

  *certificate = certificate_;
  return true;
}

bool FakeJsepTransport::SetLocalTransportDescription(
    const TransportDescription& description,
    SdpType type,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return false;
  }
  bool ice_restarting =
      local_description_set_ &&
      IceCredentialsChanged(local_description_->ice_ufrag,
                            local_description_->ice_pwd, description.ice_ufrag,
                            description.ice_pwd);
  local_description_.reset(new TransportDescription(description));

  rtc::SSLFingerprint* local_fp =
      local_description_->identity_fingerprint.get();

  if (!local_fp) {
    certificate_ = nullptr;
  } else if (!VerifyCertificateFingerprint(certificate_.get(), local_fp,
                                           error_desc)) {
    return false;
  }

  for (const auto& kv : channels_) {
    ret &= ApplyLocalTransportDescription(kv.second, error_desc);
  }
  if (!ret) {
    return false;
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    ret &= NegotiateTransportDescription(type, error_desc);
  }
  if (!ret) {
    return false;
  }

  if (needs_ice_restart_ && ice_restarting) {
    needs_ice_restart_ = false;
    RTC_LOG(LS_VERBOSE) << "needs-ice-restart flag cleared for transport "
                        << mid();
  }

  local_description_set_ = true;
  return true;
}

bool FakeJsepTransport::SetRemoteTransportDescription(
    const TransportDescription& description,
    SdpType type,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return false;
  }

  remote_description_.reset(new TransportDescription(description));
  for (const auto& kv : channels_) {
    ret &= ApplyRemoteTransportDescription(kv.second, error_desc);
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    ret = NegotiateTransportDescription(SdpType::kOffer, error_desc);
  }
  if (ret) {
    remote_description_set_ = true;
  }

  return ret;
}

bool FakeJsepTransport::VerifyCertificateFingerprint(
    const rtc::RTCCertificate* certificate,
    const rtc::SSLFingerprint* fingerprint,
    std::string* error_desc) const {
  if (!fingerprint) {
    return false;
  }
  if (!certificate) {
    return false;
  }
  std::unique_ptr<rtc::SSLFingerprint> fp_tmp(rtc::SSLFingerprint::Create(
      fingerprint->algorithm, certificate->identity()));
  RTC_DCHECK(fp_tmp.get() != NULL);
  if (*fp_tmp == *fingerprint) {
    return true;
  }
  return false;
}

bool FakeJsepTransport::ApplyLocalTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  dtls_transport->ice_transport()->SetIceParameters(
      local_description_->GetIceParameters());
  return true;
}

bool FakeJsepTransport::ApplyRemoteTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  dtls_transport->ice_transport()->SetRemoteIceParameters(
      remote_description_->GetIceParameters());
  dtls_transport->ice_transport()->SetRemoteIceMode(
      remote_description_->ice_mode);
  return true;
}

bool FakeJsepTransport::ApplyNegotiatedTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  // Set SSL role. Role must be set before fingerprint is applied, which
  // initiates DTLS setup.
  if (ssl_role_ && !dtls_transport->SetSslRole(*ssl_role_)) {
    return false;
  }
  // Apply remote fingerprint.
  if (!dtls_transport->SetRemoteFingerprint(
          remote_fingerprint_->algorithm,
          reinterpret_cast<const uint8_t*>(remote_fingerprint_->digest.data()),
          remote_fingerprint_->digest.size())) {
    return false;
  }
  return true;
}

bool FakeJsepTransport::NegotiateTransportDescription(
    SdpType local_description_type,
    std::string* error_desc) {
  if (!local_description_ || !remote_description_) {
    const std::string msg =
        "Applying an answer transport description "
        "without applying any offer.";
    return false;
  }
  rtc::SSLFingerprint* local_fp =
      local_description_->identity_fingerprint.get();
  rtc::SSLFingerprint* remote_fp =
      remote_description_->identity_fingerprint.get();
  if (remote_fp && local_fp) {
    remote_fingerprint_.reset(new rtc::SSLFingerprint(*remote_fp));
    if (!NegotiateRole(local_description_type, error_desc)) {
      return false;
    }
  } else if (local_fp && (local_description_type == SdpType::kAnswer)) {
    return false;
  } else {
    // We are not doing DTLS
    remote_fingerprint_.reset(new rtc::SSLFingerprint("", nullptr, 0));
  }
  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  for (const auto& kv : channels_) {
    if (!ApplyNegotiatedTransportDescription(kv.second, error_desc)) {
      return false;
    }
  }
  return true;
}

bool FakeJsepTransport::NegotiateRole(SdpType local_description_type,
                                      std::string* error_desc) {
  if (!local_description_ || !remote_description_) {
    const std::string msg =
        "Local and Remote description must be set before "
        "transport descriptions are negotiated";
    return false;
  }

  // From RFC 4145, section-4.1, The following are the values that the
  // 'setup' attribute can take in an offer/answer exchange:
  //       Offer      Answer
  //      ________________
  //      active     passive / holdconn
  //      passive    active / holdconn
  //      actpass    active / passive / holdconn
  //      holdconn   holdconn
  //
  // Set the role that is most conformant with RFC 5763, Section 5, bullet 1
  // The endpoint MUST use the setup attribute defined in [RFC4145].
  // The endpoint that is the offerer MUST use the setup attribute
  // value of setup:actpass and be prepared to receive a client_hello
  // before it receives the answer.  The answerer MUST use either a
  // setup attribute value of setup:active or setup:passive.  Note that
  // if the answerer uses setup:passive, then the DTLS handshake will
  // not begin until the answerer is received, which adds additional
  // latency. setup:active allows the answer and the DTLS handshake to
  // occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
  // party is active MUST initiate a DTLS handshake by sending a
  // ClientHello over each flow (host/port quartet).
  // IOW - actpass and passive modes should be treated as server and
  // active as client.
  ConnectionRole local_connection_role = local_description_->connection_role;
  ConnectionRole remote_connection_role = remote_description_->connection_role;

  bool is_remote_server = false;
  if (local_description_type == SdpType::kOffer) {
    if (local_connection_role != CONNECTIONROLE_ACTPASS) {
      return false;
    }

    if (remote_connection_role == CONNECTIONROLE_ACTIVE ||
        remote_connection_role == CONNECTIONROLE_PASSIVE ||
        remote_connection_role == CONNECTIONROLE_NONE) {
      is_remote_server = (remote_connection_role == CONNECTIONROLE_PASSIVE);
    } else {
      const std::string msg =
          "Answerer must use either active or passive value "
          "for setup attribute.";
      return false;
    }
    // If remote is NONE or ACTIVE it will act as client.
  } else {
    if (remote_connection_role != CONNECTIONROLE_ACTPASS &&
        remote_connection_role != CONNECTIONROLE_NONE) {
      // Accept a remote role attribute that's not "actpass", but matches the
      // current negotiated role. This is allowed by dtls-sdp, though our
      // implementation will never generate such an offer as it's not
      // recommended.
      //
      // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-dtls-sdp,
      // section 5.5.
      if (!ssl_role_ ||
          (*ssl_role_ == rtc::SSL_CLIENT &&
           remote_connection_role == CONNECTIONROLE_ACTIVE) ||
          (*ssl_role_ == rtc::SSL_SERVER &&
           remote_connection_role == CONNECTIONROLE_PASSIVE)) {
        return false;
      }
    }

    if (local_connection_role == CONNECTIONROLE_ACTIVE ||
        local_connection_role == CONNECTIONROLE_PASSIVE) {
      is_remote_server = (local_connection_role == CONNECTIONROLE_ACTIVE);
    } else {
      const std::string msg =
          "Answerer must use either active or passive value "
          "for setup attribute.";
      return false;
    }

    // If local is passive, local will act as server.
  }

  ssl_role_.emplace(is_remote_server ? rtc::SSL_CLIENT : rtc::SSL_SERVER);
  return true;
}

}  // namespace cricket
