/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jseptransport2.h"

#include <memory>
#include <utility>  // for std::pair

#include "api/candidate.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/p2ptransportchannel.h"
#include "p2p/base/port.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

using webrtc::SdpType;

namespace cricket {

static bool BadTransportDescription(const std::string& desc,
                                    std::string* err_desc) {
  if (err_desc) {
    *err_desc = desc;
  }
  RTC_LOG(LS_ERROR) << desc;
  return false;
}

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

JsepTransport2::JsepTransport2(
    const std::string& mid,
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate,
    SrtpMode srtp_mode,
    std::unique_ptr<webrtc::RtpTransportInternal> rtp_transport,
    std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
    std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport)
    : mid_(mid),
      certificate_(certificate),
      rtp_transport_(std::move(rtp_transport)),
      rtp_dtls_transport_(std::move(rtp_dtls_transport)),
      rtcp_dtls_transport_(std::move(rtcp_dtls_transport)) {
  RTC_DCHECK(rtp_transport_);
  RTC_DCHECK(rtp_dtls_transport_);
  switch (srtp_mode) {
    case SrtpMode::kUnencrypted:
      unencrypted_rtp_transport_ = rtp_transport_.get();
      break;
    case SrtpMode::kSdes:
      sdes_transport_ =
          static_cast<webrtc::SrtpTransport*>(rtp_transport_.get());
      break;
    case SrtpMode::kDtlsSrtp:
      dtls_srtp_transport_ =
          static_cast<webrtc::DtlsSrtpTransport*>(rtp_transport_.get());
      break;
    default:
      RTC_NOTREACHED();
      break;
  }
}

JsepTransport2::~JsepTransport2() {}

void JsepTransport2::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  certificate_ = certificate;
}

bool JsepTransport2::GetLocalCertificate(
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  if (!certificate_) {
    return false;
  }

  *certificate = certificate_;
  return true;
}

bool JsepTransport2::SetLocalTransportDescription(
    const TransportDescription& description,
    bool enable_rtcp_mux,
    const std::vector<CryptoParams>& cryptos,
    const std::vector<int>& encrypted_extension_ids,
    SdpType type,
    std::string* error_desc) {
  bool ret = true;

  ret = SetRtcpMux(enable_rtcp_mux, type, ContentSource::CS_LOCAL);
  if (!ret) {
    *error_desc = "Failed to setup RTCP mux.";
    return ret;
  }

  // If doing SDES, setup the SDES crypto parameters.
  if (sdes_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!dtls_srtp_transport_);
    ret = SetSdes(cryptos, encrypted_extension_ids, type,
                  ContentSource::CS_LOCAL);
    if (!ret) {
      *error_desc = "Failed to setup SDES crypto parameters.";
      return ret;
    }
  } else if (dtls_srtp_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!sdes_transport_);

    dtls_srtp_transport_->UpdateRecvEncryptedHeaderExtensionIds(
        encrypted_extension_ids);
  }

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
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

  ret &= ApplyLocalTransportDescription(rtp_dtls_transport_.get(), error_desc);
  if (rtcp_dtls_transport_) {
    ret &=
        ApplyLocalTransportDescription(rtcp_dtls_transport_.get(), error_desc);
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

bool JsepTransport2::SetRemoteTransportDescription(
    const TransportDescription& description,
    bool enable_rtcp_mux,
    const std::vector<CryptoParams>& cryptos,
    const std::vector<int>& encrypted_extension_ids,
    webrtc::SdpType type,
    std::string* error_desc) {
  bool ret = true;

  ret = SetRtcpMux(enable_rtcp_mux, type, ContentSource::CS_REMOTE);
  if (!ret) {
    *error_desc = "Failed to setup RTCP mux.";
    return ret;
  }

  // If doing SDES, setup the SDES crypto parameters.
  if (sdes_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!dtls_srtp_transport_);
    ret = SetSdes(cryptos, encrypted_extension_ids, type,
                  ContentSource::CS_REMOTE);
    if (!ret) {
      *error_desc = "Failed to setup SDES crypto parameters.";
      return ret;
    }
  } else if (dtls_srtp_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!sdes_transport_);

    dtls_srtp_transport_->UpdateSendEncryptedHeaderExtensionIds(
        encrypted_extension_ids);
  }

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  remote_description_.reset(new TransportDescription(description));
  ret &= ApplyRemoteTransportDescription(rtp_dtls_transport_.get(), error_desc);
  if (rtcp_dtls_transport_) {
    ret &=
        ApplyRemoteTransportDescription(rtcp_dtls_transport_.get(), error_desc);
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

void JsepTransport2::SetNeedsIceRestartFlag() {
  if (!needs_ice_restart_) {
    needs_ice_restart_ = true;
    RTC_LOG(LS_VERBOSE) << "needs-ice-restart flag set for transport " << mid();
  }
}

bool JsepTransport2::NeedsIceRestart() const {
  return needs_ice_restart_;
}

rtc::Optional<rtc::SSLRole> JsepTransport2::GetSslRole() const {
  return ssl_role_;
}

bool JsepTransport2::GetStats(TransportStats* stats) {
  stats->transport_name = mid();
  stats->channel_stats.clear();
  bool ret = GetTransportStats(cricket::ICE_CANDIDATE_COMPONENT_RTP, stats);
  if (rtcp_dtls_transport_) {
    ret &= GetTransportStats(cricket::ICE_CANDIDATE_COMPONENT_RTCP, stats);
  }
  return ret;
}

bool JsepTransport2::VerifyCertificateFingerprint(
    const rtc::RTCCertificate* certificate,
    const rtc::SSLFingerprint* fingerprint,
    std::string* error_desc) const {
  if (!fingerprint) {
    return BadTransportDescription("No fingerprint.", error_desc);
  }
  if (!certificate) {
    return BadTransportDescription(
        "Fingerprint provided but no identity available.", error_desc);
  }
  std::unique_ptr<rtc::SSLFingerprint> fp_tmp(rtc::SSLFingerprint::Create(
      fingerprint->algorithm, certificate->identity()));
  RTC_DCHECK(fp_tmp.get() != NULL);
  if (*fp_tmp == *fingerprint) {
    return true;
  }
  std::ostringstream desc;
  desc << "Local fingerprint does not match identity. Expected: ";
  desc << fp_tmp->ToString();
  desc << " Got: " << fingerprint->ToString();
  return BadTransportDescription(desc.str(), error_desc);
}

bool JsepTransport2::ApplyLocalTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  RTC_DCHECK(dtls_transport);
  dtls_transport->ice_transport()->SetIceParameters(
      local_description_->GetIceParameters());
  return true;
}

bool JsepTransport2::ApplyRemoteTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  RTC_DCHECK(dtls_transport);
  dtls_transport->ice_transport()->SetRemoteIceParameters(
      remote_description_->GetIceParameters());
  dtls_transport->ice_transport()->SetRemoteIceMode(
      remote_description_->ice_mode);
  return true;
}

bool JsepTransport2::ApplyNegotiatedTransportDescription(
    DtlsTransportInternal* dtls_transport,
    std::string* error_desc) {
  RTC_DCHECK(dtls_transport);
  // Set SSL role. Role must be set before fingerprint is applied, which
  // initiates DTLS setup.
  if (ssl_role_ && !dtls_transport->SetSslRole(*ssl_role_)) {
    return BadTransportDescription("Failed to set SSL role for the channel.",
                                   error_desc);
  }
  // Apply remote fingerprint.
  if (!dtls_transport->SetRemoteFingerprint(
          remote_fingerprint_->algorithm,
          reinterpret_cast<const uint8_t*>(remote_fingerprint_->digest.data()),
          remote_fingerprint_->digest.size())) {
    return BadTransportDescription("Failed to apply remote fingerprint.",
                                   error_desc);
  }
  return true;
}

bool JsepTransport2::SetRtcpMux(bool enable,
                                webrtc::SdpType type,
                                ContentSource source) {
  bool ret = false;
  switch (type) {
    case SdpType::kOffer:
      ret = rtcp_mux_negotiator_.SetOffer(enable, source);
      break;
    case SdpType::kPrAnswer:
      // This may activate RTCP muxing, but we don't yet destroy the transport
      // because the final answer may deactivate it.
      ret = rtcp_mux_negotiator_.SetProvisionalAnswer(enable, source);
      break;
    case SdpType::kAnswer:
      ret = rtcp_mux_negotiator_.SetAnswer(enable, source);
      ActivateRtcpMux();
      break;
    default:
      RTC_NOTREACHED();
  }

  auto rtp_transport = GetRtpTransport();
  if (ret && rtp_transport) {
    rtp_transport->SetRtcpMuxEnabled(rtcp_mux_negotiator_.IsActive());
  }
  return ret;
}

void JsepTransport2::ActivateRtcpMux() {
  if (unencrypted_rtp_transport_) {
    RTC_DCHECK(!sdes_transport_);
    RTC_DCHECK(!dtls_srtp_transport_);
    unencrypted_rtp_transport_->SetRtcpPacketTransport(nullptr);
  } else if (sdes_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!dtls_srtp_transport_);
    sdes_transport_->SetRtcpPacketTransport(nullptr);
  } else {
    RTC_DCHECK(dtls_srtp_transport_);
    RTC_DCHECK(!unencrypted_rtp_transport_);
    RTC_DCHECK(!sdes_transport_);
    dtls_srtp_transport_->SetDtlsTransports(GetRtpDtlsTransport(),
                                            /*rtcp_dtls_transport=*/nullptr);
  }
  rtcp_dtls_transport_.reset();
  // Notify the JsepTransportController to update the aggregate states.
  SignalRtcpMuxFullyActive();
}

bool JsepTransport2::SetSdes(const std::vector<CryptoParams>& cryptos,
                             const std::vector<int>& encrypted_extension_ids,
                             webrtc::SdpType type,
                             ContentSource source) {
  bool ret = false;
  ret =
      sdes_negotiator_.Process(cryptos, encrypted_extension_ids, type, source);
  if (!ret) {
    return ret;
  }

  // If setting an SDES answer succeeded, apply the negotiated parameters
  // to the SRTP transport.
  if ((type == SdpType::kPrAnswer || type == SdpType::kAnswer) && ret) {
    if (sdes_negotiator_.send_cipher_suite() &&
        sdes_negotiator_.recv_cipher_suite()) {
      RTC_DCHECK(sdes_negotiator_.send_extension_ids());
      RTC_DCHECK(sdes_negotiator_.recv_extension_ids());
      ret = sdes_transport_->SetRtpParams(
          *(sdes_negotiator_.send_cipher_suite()),
          sdes_negotiator_.send_key().data(),
          static_cast<int>(sdes_negotiator_.send_key().size()),
          *(sdes_negotiator_.send_extension_ids()),
          *(sdes_negotiator_.recv_cipher_suite()),
          sdes_negotiator_.recv_key().data(),
          static_cast<int>(sdes_negotiator_.recv_key().size()),
          *(sdes_negotiator_.recv_extension_ids()));
    } else {
      RTC_LOG(LS_INFO) << "No crypto keys are provided for SDES.";
      if (type == SdpType::kAnswer) {
        // Explicitly reset the |sdes_transport_| if no crypto param is
        // provided in the answer. No need to call |ResetParams()| for
        // |sdes_negotiator_| because it resets the params inside |SetAnswer|.
        sdes_transport_->ResetParams();
      }
    }
  }
  return ret;
}

bool JsepTransport2::NegotiateTransportDescription(
    SdpType local_description_type,
    std::string* error_desc) {
  if (!local_description_ || !remote_description_) {
    const std::string msg =
        "Applying an answer transport description "
        "without applying any offer.";
    return BadTransportDescription(msg, error_desc);
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
    return BadTransportDescription(
        "Local fingerprint supplied when caller didn't offer DTLS.",
        error_desc);
  } else {
    // We are not doing DTLS
    remote_fingerprint_.reset(new rtc::SSLFingerprint("", nullptr, 0));
  }
  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  bool ret = ApplyNegotiatedTransportDescription(rtp_dtls_transport_.get(),
                                                 error_desc);
  if (rtcp_dtls_transport_) {
    ret &= ApplyNegotiatedTransportDescription(rtcp_dtls_transport_.get(),
                                               error_desc);
  }
  return ret;
}

bool JsepTransport2::NegotiateRole(SdpType local_description_type,
                                   std::string* error_desc) {
  if (!local_description_ || !remote_description_) {
    const std::string msg =
        "Local and Remote description must be set before "
        "transport descriptions are negotiated";
    return BadTransportDescription(msg, error_desc);
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
      return BadTransportDescription(
          "Offerer must use actpass value for setup attribute.", error_desc);
    }

    if (remote_connection_role == CONNECTIONROLE_ACTIVE ||
        remote_connection_role == CONNECTIONROLE_PASSIVE ||
        remote_connection_role == CONNECTIONROLE_NONE) {
      is_remote_server = (remote_connection_role == CONNECTIONROLE_PASSIVE);
    } else {
      const std::string msg =
          "Answerer must use either active or passive value "
          "for setup attribute.";
      return BadTransportDescription(msg, error_desc);
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
        return BadTransportDescription(
            "Offerer must use actpass value or current negotiated role for "
            "setup attribute.",
            error_desc);
      }
    }

    if (local_connection_role == CONNECTIONROLE_ACTIVE ||
        local_connection_role == CONNECTIONROLE_PASSIVE) {
      is_remote_server = (local_connection_role == CONNECTIONROLE_ACTIVE);
    } else {
      const std::string msg =
          "Answerer must use either active or passive value "
          "for setup attribute.";
      return BadTransportDescription(msg, error_desc);
    }

    // If local is passive, local will act as server.
  }

  ssl_role_.emplace(is_remote_server ? rtc::SSL_CLIENT : rtc::SSL_SERVER);
  return true;
}

bool JsepTransport2::GetTransportStats(int component, TransportStats* stats) {
  auto dtls_transport = component == cricket::ICE_CANDIDATE_COMPONENT_RTP
                            ? rtp_dtls_transport_.get()
                            : rtcp_dtls_transport_.get();
  RTC_DCHECK(dtls_transport);
  TransportChannelStats substats;
  substats.component = component;
  dtls_transport->GetSrtpCryptoSuite(&substats.srtp_crypto_suite);
  dtls_transport->GetSslCipherSuite(&substats.ssl_cipher_suite);
  substats.dtls_state = dtls_transport->dtls_state();
  if (!dtls_transport->ice_transport()->GetStats(&substats.connection_infos)) {
    return false;
  }
  stats->channel_stats.push_back(substats);
  return true;
}

}  // namespace cricket
