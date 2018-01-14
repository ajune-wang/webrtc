/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
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

using cricket::IceTransportInternal;
using cricket::DtlsTransportInternal;
using cricket::DtlsTransport;
using cricket::JsepTransport;
using cricket::ContentInfo;
using cricket::IceRole;
using cricket::IceTransportState;
using cricket::TransportStats;
using cricket::Candidate;
using cricket::Candidates;
using cricket::PortAllocator;
using cricket::BaseChannel;
using cricket::IceGatheringState;
using cricket::IceConnectionState;
using cricket::ICEROLE_CONTROLLING;
using cricket::ICEROLE_CONTROLLED;
using cricket::ICEMODE_LITE;
using cricket::ICEMODE_FULL;
using cricket::PRFLX_PORT_TYPE;
using cricket::kIceGatheringComplete;
using cricket::kIceGatheringNew;
using cricket::kIceGatheringGathering;
using cricket::kIceConnectionFailed;
using cricket::kIceConnectionCompleted;
using cricket::kIceConnectionConnecting;
using cricket::kIceConnectionConnected;

class JsepTransportController : public sigslot::has_slots<>,
                                public rtc::MessageHandler {
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
    bool max_bundle_required;
    bool rtcp_mux_required;
    bool disable_encryption;
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
  RTCError ApplyDescription(cricket::ContentSource source,
                            SdpType type,
                            const cricket::SessionDescription* description);

  // Get transports to be used for the provided |mid|. If bundling is enabled,
  // calling GetRtpTransport for multiple MIDs may yield the same object.
  RtpTransportInternal* GetRtpTransport(const std::string& mid);
  cricket::DtlsTransportInternal* GetDtlsTransport(const std::string& mid);

  // Add BaseChannel/SctpTransport objects as being associated with a
  // particular MID. If BUNDLE negotiation changes which transport they should
  // use, JsepTransportController will handle calling the necessary
  // "SetTransport" method.
  bool AddChannel(const std::string& mid, cricket::BaseChannel* channel);
  void AddSctpTransport(const std::string& mid,
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

  // Decrements a channel's reference count, and destroys the channel if
  // nothing is referencing it.
  void DestroyDtlsTransport_n(const std::string& transport_name, int component);

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

 protected:
  // TODO(deadbeef): Get rid of these virtual methods. Used by
  // FakeTransportController currently, but FakeTransportController shouldn't
  // even be functioning by subclassing TransportController.
  virtual IceTransportInternal* CreateIceTransportChannel_n(
      const std::string& transport_name,
      int component);
  virtual DtlsTransportInternal* CreateDtlsTransportChannel_n(
      const std::string& transport_name,
      int component,
      IceTransportInternal* ice);

 private:
  void OnMessage(rtc::Message* pmsg) override;

  class ChannelPair;
  typedef rtc::RefCountedObject<ChannelPair> RefCountedChannel;

  // Wrapper for RtpTransport that keeps a reference count.
  // When using SDES, |srtp_transport| is non-null, |dtls_srtp_transport| is
  // null and |rtp_transport.get()| == |srtp_transport|,
  // When using DTLS-SRTP, |dtls_srtp_transport| is non-null, |srtp_transport|
  // is null and |rtp_transport.get()| == |dtls_srtp_transport|,
  // When using unencrypted RTP, only |rtp_transport| is non-null.
  struct RtpTransportWrapper {
    // |rtp_transport| is always non-null.
    std::unique_ptr<webrtc::RtpTransportInternal> rtp_transport;
    webrtc::SrtpTransport* srtp_transport = nullptr;
    webrtc::DtlsSrtpTransport* dtls_srtp_transport = nullptr;
  };

  typedef rtc::RefCountedObject<RtpTransportWrapper> RefCountedRtpTransport;

  const RefCountedRtpTransport* FindRtpTransport(
      const std::string& transport_name);

  bool ShouldEnableBundle(cricket::ContentSource source,
                          SdpType type,
                          const cricket::SessionDescription* description);

  void EnableBundleForSctpTransport_n();

  std::vector<int> MergeEncryptedHeaderExtensionIdsForBundle(
      const cricket::SessionDescription* description);

  std::vector<int> GetEncryptedHeaderExtensionIds(
      const ContentInfo& content_info);

  // Helper functions to get a channel or transport, or iterator to it (in
  // case it needs to be erased).
  std::vector<RefCountedChannel*>::iterator GetChannelIterator_n(
      const std::string& transport_name,
      int component);
  std::vector<RefCountedChannel*>::const_iterator GetChannelIterator_n(
      const std::string& transport_name,
      int component) const;
  const JsepTransport* GetJsepTransport(
      const std::string& transport_name) const;
  JsepTransport* GetJsepTransport(const std::string& transport_name);

  void MaybeCreateJsepTransport(const std::string& mid,
                                const ContentInfo& content_info);

  const RefCountedChannel* GetChannel_n(const std::string& transport_name,
                                        int component) const;
  RefCountedChannel* GetChannel_n(const std::string& transport_name,
                                  int component);
  void DestroyAllChannels_n();

  void SetIceConfig_n(const cricket::IceConfig& config);
  void SetIceRole_n(IceRole ice_role);
  bool GetSslRole_n(const std::string& transport_name,
                    rtc::SSLRole* role) const;
  bool SetLocalCertificate_n(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate_n(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate_n(
      const std::string& transport_name) const;

  void MaybeStartGathering_n();
  bool AddRemoteCandidates_n(const std::string& transport_name,
                             const Candidates& candidates,
                             std::string* err);
  bool RemoveRemoteCandidates_n(const Candidates& candidates, std::string* err);
  bool ReadyForRemoteCandidates_n(const std::string& transport_name) const;
  bool GetStats_n(const std::string& transport_name, TransportStats* stats);
  void SetMetricsObserver_n(webrtc::MetricsObserverInterface* metrics_observer);

  bool SetLocalRtpTransportDescription_n(
      const std::string& mid,
      const cricket::ContentInfo& content_info,
      const cricket::TransportInfo& transport_info,
      SdpType type,
      const std::vector<int> encrypted_extension_ids,
      std::string* error);
  bool SetRemoteRtpTransportDescription_n(
      const std::string& mid,
      const cricket::ContentInfo& content_info,
      const cricket::TransportInfo& transport_info,
      SdpType type,
      const std::vector<int> encrypted_extension_ids,
      std::string* error);

  // Creates a channel if it doesn't exist. Otherwise, increments a reference
  // count and returns an existing channel.
  DtlsTransportInternal* CreateDtlsTransport_n(
      const std::string& transport_name,
      int component);

  void DestroyRtcpTransport_n(const std::string& mid);

  // Create an SrtpTransport/DtlsSrtpTransport if it doesn't exist.
  // Otherwise, increments a reference count and returns the existing one.
  // These methods are not currently used but the plan is to transition
  // PeerConnection and BaseChannel to use them instead of CreateDtlsTransport.
  webrtc::RtpTransport* CreateUnencryptedRtpTransport(
      const std::string& transport_name,
      bool rtcp_mux_enabled);
  webrtc::SrtpTransport* CreateSdesTransport(const std::string& transport_name,
                                             bool rtcp_mux_enabled);
  webrtc::DtlsSrtpTransport* CreateDtlsSrtpTransport(
      const std::string& transport_name,
      bool rtcp_mux_enabled);

  // Destroy an RTP level transport which can be an RtpTransport, an
  // SrtpTransport or a DtlsSrtpTransport.
  void DestroyTransport(const std::string& transport_name);

  // Handlers for signals from Transport.
  void OnChannelWritableState_n(rtc::PacketTransportInternal* transport);
  void OnChannelReceivingState_n(rtc::PacketTransportInternal* transport);
  void OnChannelGatheringState_n(IceTransportInternal* channel);
  void OnChannelCandidateGathered_n(IceTransportInternal* channel,
                                    const Candidate& candidate);
  void OnChannelCandidatesRemoved(const Candidates& candidates);
  void OnChannelCandidatesRemoved_n(IceTransportInternal* channel,
                                    const Candidates& candidates);
  void OnChannelRoleConflict_n(IceTransportInternal* channel);
  void OnChannelStateChanged_n(IceTransportInternal* channel);

  void UpdateAggregateStates_n();

  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  rtc::Thread* const signaling_thread_ = nullptr;
  rtc::Thread* const network_thread_ = nullptr;
  cricket::PortAllocator* const port_allocator_ = nullptr;

  std::map<std::string, std::unique_ptr<JsepTransport>> transports_;
  std::vector<RefCountedChannel*> channels_;

  std::map<std::string, RefCountedRtpTransport*> rtp_transports_;

  // Aggregate state for TransportChannelImpls.
  cricket::IceConnectionState connection_state_ =
      cricket::kIceConnectionConnecting;
  bool receiving_ = false;
  cricket::IceGatheringState gathering_state_ = cricket::kIceGatheringNew;

  Config config_;
  const cricket::SessionDescription* local_desc_ = nullptr;
  const cricket::SessionDescription* remote_desc_ = nullptr;

  bool bundle_enabled_ = false;
  std::string bundled_mid_;
  const cricket::ContentGroup* bundle_group_ = nullptr;

  cricket::IceConfig ice_config_;
  IceRole ice_role_ = cricket::ICEROLE_CONTROLLING;
  bool redetermine_role_on_ice_restart_;
  uint64_t ice_tiebreaker_ = rtc::CreateRandomId64();
  rtc::CryptoOptions crypto_options_;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::AsyncInvoker invoker_;

  std::map<std::string, cricket::BaseChannel*> channels_by_mid_;
  std::string sctp_mid_;
  cricket::SctpTransportInternal* sctp_transport_ = nullptr;

  webrtc::MetricsObserverInterface* metrics_observer_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepTransportController);
};

}  // namespace webrtc

#endif  // PC_JSEPTRANSPORTCONTROLLER_H_
