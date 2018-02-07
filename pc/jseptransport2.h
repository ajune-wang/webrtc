/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEPTRANSPORT2_H_
#define PC_JSEPTRANSPORT2_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "api/jsep.h"
#include "api/optional.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/transportinfo.h"
#include "pc/dtlssrtptransport.h"
#include "pc/rtcpmuxfilter.h"
#include "pc/rtptransport.h"
#include "pc/sessiondescription.h"
#include "pc/srtpfilter.h"
#include "pc/srtptransport.h"
#include "pc/transportstats.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/sslstreamadapter.h"

namespace cricket {

class DtlsTransportInternal;

enum class SrtpMode {
  kUnencrypted,
  kSdes,
  kDtlsSrtp,
};

// Helper class used by JsepTransportController that processes
// TransportDescriptions. A TransportDescription represents the
// transport-specific properties of an SDP m= section, processed according to
// JSEP. Each transport consists of DTLS and ICE transport channels for RTP
// (and possibly RTCP, if rtcp-mux isn't used).
//
// On Threading:  JsepTransport performs work solely on the network thread, and
// so its methods should only be called on the network thread.
class JsepTransport2 : public sigslot::has_slots<> {
 public:
  // |mid| is just used for log statements in order to identify the Transport.
  // Note that |local_certificate| is allowed to be null since a remote
  // description may be set before a local certificate is generated.
  JsepTransport2(
      const std::string& mid,
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate,
      SrtpMode srtp_mode,
      std::unique_ptr<webrtc::RtpTransportInternal> rtp_transport,
      std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
      std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport);

  ~JsepTransport2() override;

  // Returns the MID of this transport.
  const std::string& mid() const { return mid_; }

  bool ready_for_remote_candidates() const {
    return local_description_set_ && remote_description_set_;
  }

  // Must be called before applying local session description.
  // Needed in order to verify the local fingerprint.
  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate);

  // Get a copy of the local certificate provided by SetLocalCertificate.
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* local_certificate) const;

  // Set the local TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  webrtc::RTCError SetLocalTransportDescription(
      const TransportDescription& description,
      bool enable_rtcp_mux,
      const std::vector<CryptoParams>& cryptos,
      const std::vector<int>& encrypted_extension_ids,
      webrtc::SdpType type);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  webrtc::RTCError SetRemoteTransportDescription(
      const TransportDescription& description,
      bool enable_rtcp_mux,
      const std::vector<CryptoParams>& cryptos,
      const std::vector<int>& encrypted_extension_ids,
      webrtc::SdpType type);

  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  //
  // This and the below method can be called safely from any thread as long as
  // SetXTransportDescription is not in progress.
  void SetNeedsIceRestartFlag();
  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password).
  bool NeedsIceRestart() const;

  // Returns role if negotiated, or empty Optional if it hasn't been negotiated
  // yet.
  rtc::Optional<rtc::SSLRole> GetSslRole() const;

  // TODO(deadbeef): Make this const. See comment in transportcontroller.h.
  bool GetStats(TransportStats* stats);

  // The current local transport description, possibly used
  // by the transport controller.
  const TransportDescription* local_description() const {
    return local_description_.get();
  }

  // The current remote transport description, possibly used
  // by the transport controller.
  const TransportDescription* remote_description() const {
    return remote_description_.get();
  }

  webrtc::RtpTransportInternal* GetRtpTransport() const {
    return rtp_transport_.get();
  }

  DtlsTransportInternal* GetDtlsTransport() const {
    return rtp_dtls_transport_.get();
  }

  DtlsTransportInternal* GetRtcpDtlsTransport() const {
    return rtcp_dtls_transport_.get();
  }

  // This is signaled when the RTCP-mux becomes fully active and
  // |rtcp_dtls_transport_| is destroyed. The JsepTransportController will
  // handle the signal and update the aggregate transport states.
  sigslot::signal<> SignalRtcpMuxFullyActive;

  // TODO(deadbeef): The methods below are only public for testing. Should make
  // them utility functions or objects so they can be tested independently from
  // this class.
  // TODO(zhihuang): Change the methods that currently use a bool return and
  // |error_desc| out parameter to use RTCError instead.

  // Returns false if the certificate's identity does not match the fingerprint,
  // or either is NULL.
  bool VerifyCertificateFingerprint(const rtc::RTCCertificate* certificate,
                                    const rtc::SSLFingerprint* fingerprint,
                                    std::string* error_desc) const;

 private:
  bool SetRtcpMux(bool enable, webrtc::SdpType type, ContentSource source);

  void ActivateRtcpMux();

  bool SetSdes(const std::vector<CryptoParams>& cryptos,
               const std::vector<int>& encrypted_extension_ids,
               webrtc::SdpType type,
               ContentSource source);

  // Negotiates the transport parameters based on the current local and remote
  // transport description, such as the ICE role to use, and whether DTLS
  // should be activated.
  //
  // Called when an answer TransportDescription is applied.
  bool NegotiateTransportDescription(webrtc::SdpType local_description_type,
                                     std::string* error_desc);

  // Negotiates the SSL role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns false if the SSL role cannot be determined
  // from the local description and remote description.
  bool NegotiateRole(webrtc::SdpType local_description_type,
                     std::string* error_desc);

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  bool ApplyLocalTransportDescription(DtlsTransportInternal* dtls_transport,
                                      std::string* error_desc);

  // Pushes down the transport parameters from the remote description to the
  // transport channel.
  bool ApplyRemoteTransportDescription(DtlsTransportInternal* dtls_transport,
                                       std::string* error_desc);

  // Pushes down the transport parameters obtained via negotiation.
  bool ApplyNegotiatedTransportDescription(
      DtlsTransportInternal* dtls_transport,
      std::string* error_desc);

  bool GetTransportStats(int component, TransportStats* stats);

  const std::string mid_;
  // needs-ice-restart bit as described in JSEP.
  bool needs_ice_restart_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  rtc::Optional<rtc::SSLRole> ssl_role_;
  std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint_;
  std::unique_ptr<TransportDescription> local_description_;
  std::unique_ptr<TransportDescription> remote_description_;
  bool local_description_set_ = false;
  bool remote_description_set_ = false;

  std::unique_ptr<webrtc::RtpTransportInternal> rtp_transport_;
  webrtc::RtpTransportInternal* unencrypted_rtp_transport_ = nullptr;
  webrtc::SrtpTransport* sdes_transport_ = nullptr;
  webrtc::DtlsSrtpTransport* dtls_srtp_transport_ = nullptr;

  std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport_;
  std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport_;

  SrtpFilter sdes_negotiator_;
  RtcpMuxFilter rtcp_mux_negotiator_;

  rtc::Optional<std::vector<int>> send_extension_ids_;
  rtc::Optional<std::vector<int>> recv_extension_ids_;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepTransport2);
};

}  // namespace cricket

#endif  // PC_JSEPTRANSPORT2_H_
