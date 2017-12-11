/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_FAKEJSEPTRANSPORT_H_
#define P2P_BASE_FAKEJSEPTRANSPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "api/jsep.h"
#include "api/optional.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/sessiondescription.h"
#include "p2p/base/transporthelper.h"
#include "p2p/base/transportinfo.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/sslstreamadapter.h"

namespace cricket {

class DtlsTransportInternal;

// This class is only used by DtlsTransport unit tests.
// TODO(zhihuang): Remove this class when the DtlsTransport tests are updated.
class FakeJsepTransport : public sigslot::has_slots<> {
 public:
  FakeJsepTransport(const std::string& mid,
                    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  ~FakeJsepTransport() override;

  // Returns the MID of this transport.
  const std::string& mid() const { return mid_; }

  // Add or remove channel that is affected when a local/remote transport
  // description is set on this transport. Need to add all channels before
  // setting a transport description.
  bool AddChannel(DtlsTransportInternal* dtls, int component);

  bool ready_for_remote_candidates() const {
    return local_description_set_ && remote_description_set_;
  }

  // Get a copy of the local certificate provided by SetLocalCertificate.
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;

  // Set the local TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    webrtc::SdpType type,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     webrtc::SdpType type,
                                     std::string* error_desc);

  // Returns role if negotiated, or empty Optional if it hasn't been negotiated
  // yet.
  rtc::Optional<rtc::SSLRole> GetSslRole() const;

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

  // TODO(deadbeef): The methods below are only public for testing. Should make
  // them utility functions or objects so they can be tested independently from
  // this class.

  // Returns false if the certificate's identity does not match the fingerprint,
  // or either is NULL.
  bool VerifyCertificateFingerprint(const rtc::RTCCertificate* certificate,
                                    const rtc::SSLFingerprint* fingerprint,
                                    std::string* error_desc) const;

 private:
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

  const std::string mid_;
  // needs-ice-restart bit as described in JSEP.
  bool needs_ice_restart_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::Optional<rtc::SSLRole> ssl_role_;
  std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint_;
  std::unique_ptr<TransportDescription> local_description_;
  std::unique_ptr<TransportDescription> remote_description_;
  bool local_description_set_ = false;
  bool remote_description_set_ = false;

  // Candidate component => DTLS channel
  std::map<int, DtlsTransportInternal*> channels_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FakeJsepTransport);
};

}  // namespace cricket

#endif  // P2P_BASE_FAKEJSEPTRANSPORT_H_
