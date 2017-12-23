/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEPTRANSPORTCONTROLLER_H_
#define PC_JSEPTRANSPORTCONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "media/sctp/sctptransportinternal.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2ptransportchannel.h"
#include "pc/channel.h"
#include "pc/dtlssrtptransport.h"
#include "pc/jseptransport.h"
#include "pc/rtptransport.h"
#include "pc/srtptransport.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/sslstreamadapter.h"

namespace rtc {
class Thread;
class PacketTransportInternal;
}  // namespace rtc

namespace webrtc {

// TODO: Use RTCError instead of bool returns?
class JsepTransportController {
 public:
  struct Config {
    // If |redetermine_role_on_ice_restart| is true, ICE role is redetermined
    // upon setting a local transport description that indicates an ICE
    // restart.
    bool redetermine_role_on_ice_restart = true;
    rtc::SSLProtocolVersion ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
    // |crypto_options| is used to determine if created DTLS transports
    // negotiate GCM crypto suites or not.
    rtc::CryptoOptions crypto_options;
  };

  JsepTransportController(rtc::Thread* signaling_thread,
                          rtc::Thread* network_thread,
                          cricket::PortAllocator* port_allocator,
                          Config config);
  virtual ~JsepTransportController();

  // The main method to be called; applies a description at the transport
  // level, creating/destroying transport objects as needed and updating their
  // properties. This includes RTP, DTLS, and ICE (but not SCTP). At least not
  // yet? May make sense to in the future.
  bool ApplyDescription(cricket::ContentSource source,
                        SdpType type,
                        const cricket::SessionDescription* description,
                        std::string* error);

  // Get transports to be used for the provided |mid|. If bundling is enabled,
  // calling GetRtpTransport for multiple MIDs may yield the same object.
  RtpTransportInternal* GetRtpTransport(const std::string& mid);
  cricket::DtlsTransportInternal* GetDtlsTransport(const std::string& mid);

  // Add BaseChannel/SctpTransport objects as being associated with a
  // particular MID. If BUNDLE negotiation changes which transport they should
  // use, JsepTransportController will handle calling the necessary
  // "SetTransport" method.
  bool AddChannel(const std::string& mid, cricket::BaseChannel* channel);
  bool AddSctpTransport(const std::string& mid,
                        cricket::SctpTransportInternal* sctp_transport);

  /*********************
   * ICE-related methods
   ********************/
  void SetIceConfig(const cricket::IceConfig& config);
  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  void SetNeedsIceRestartFlag();
  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password). If the transport has been deleted as a result of
  // bundling, returns false.
  bool NeedsIceRestart(const std::string& mid) const;
  // Start gathering candidates for any new transports, or transports doing an
  // ICE restart.
  void MaybeStartGathering();
  bool AddRemoteCandidates(const std::string& mid,
                           const std::vector<cricket::Candidate>& candidates,
                           std::string* err);
  bool RemoveRemoteCandidates(const std::vector<cricket::Candidate>& candidates,
                              std::string* err);
  bool ReadyForRemoteCandidates(const std::string& mid) const;

  /**********************
   * DTLS-related methods
   *********************/
  // Specifies the identity to use in this session.
  // Can only be called once.
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate(
      const std::string& mid,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;
  // Caller owns returned certificate. This method mainly exists for stats
  // reporting.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& mid) const;
  // Get negotiated role, if one has been negotiated.
  bool GetSslRole(const std::string& mid, rtc::SSLRole* role) const;

  // TODO(deadbeef): GetStats isn't const because all the way down to
  // OpenSSLStreamAdapter,
  // GetSslCipherSuite and GetDtlsSrtpCryptoSuite are not const. Fix this.
  bool GetStats(const std::string& mid, cricket::TransportStats* stats);
  void SetMetricsObserver(webrtc::MetricsObserverInterface* metrics_observer);

  // All of these signals are fired on the signaling thread.

  // If any transport failed => failed,
  // Else if all completed => completed,
  // Else if all connected => connected,
  // Else => connecting
  sigslot::signal1<cricket::IceConnectionState> SignalConnectionState;

  // Receiving if any transport is receiving
  sigslot::signal1<bool> SignalReceiving;

  // If all transports done gathering => complete,
  // Else if any are gathering => gathering,
  // Else => new
  sigslot::signal1<cricket::IceGatheringState> SignalGatheringState;

  // (mid, candidates)
  sigslot::signal2<const std::string&, const std::vector<cricket::Candidate>&>
      SignalCandidatesGathered;

  sigslot::signal1<const std::vector<cricket::Candidate>&>
      SignalCandidatesRemoved;

  sigslot::signal1<rtc::SSLHandshakeError> SignalDtlsHandshakeError;
};

}  // namespace webrtc

#endif  // PC_JSEPTRANSPORTCONTROLLER_H_
