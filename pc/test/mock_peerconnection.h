/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_PEERCONNECTION_H_
#define PC_TEST_MOCK_PEERCONNECTION_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "pc/peerconnection.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"

namespace webrtc {

// The factory isn't really used; it just satisfies the base PeerConnection.
class FakePeerConnectionFactory
    : public rtc::RefCountedObject<webrtc::PeerConnectionFactory> {
 public:
  explicit FakePeerConnectionFactory(
      std::unique_ptr<cricket::MediaEngineInterface> media_engine)
      : rtc::RefCountedObject<webrtc::PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            std::move(media_engine),
            std::unique_ptr<webrtc::CallFactoryInterface>(),
            std::unique_ptr<RtcEventLogFactoryInterface>()) {}
};

class MockPeerConnection
    : public rtc::RefCountedObject<webrtc::PeerConnection> {
 public:
  // TODO(nisse): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  explicit MockPeerConnection(PeerConnectionFactory* factory)
      : rtc::RefCountedObject<webrtc::PeerConnection>(
            factory,
            std::unique_ptr<RtcEventLog>(),
            std::unique_ptr<Call>()) {}

  // PeerConnectionInterface
  MOCK_METHOD0(local_streams, rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD0(remote_streams, rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD1(AddStream, bool(MediaStreamInterface* stream));
  MOCK_METHOD1(RemoveStream, void(MediaStreamInterface* stream));
  MOCK_METHOD2(AddTrack,
               RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface> track,
                   const std::vector<std::string>& stream_ids));
  MOCK_METHOD2(AddTrack,
               rtc::scoped_refptr<RtpSenderInterface>(
                   MediaStreamTrackInterface* track,
                   std::vector<MediaStreamInterface*> streams));
  MOCK_METHOD1(RemoveTrack, bool(RtpSenderInterface* sender));
  MOCK_METHOD1(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface> track));
  MOCK_METHOD2(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface> track,
                   const RtpTransceiverInit& init));
  MOCK_METHOD1(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   cricket::MediaType media_type));
  MOCK_METHOD2(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   cricket::MediaType media_type,
                   const RtpTransceiverInit& init));
  MOCK_METHOD2(
      CreateSender,
      rtc::scoped_refptr<RtpSenderInterface>(const std::string& kind,
                                             const std::string& stream_id));
  MOCK_CONST_METHOD0(GetSenders,
                     std::vector<rtc::scoped_refptr<RtpSenderInterface>>());
  MOCK_CONST_METHOD0(GetReceivers,
                     std::vector<rtc::scoped_refptr<RtpReceiverInterface>>());
  MOCK_CONST_METHOD0(
      GetTransceivers,
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>());
  MOCK_METHOD3(GetStats,
               bool(StatsObserver* observer,
                    MediaStreamTrackInterface* track,
                    StatsOutputLevel level));
  MOCK_METHOD1(GetStats, void(RTCStatsCollectorCallback* callback));
  MOCK_METHOD2(GetStats,
               void(rtc::scoped_refptr<RtpSenderInterface> selector,
                    rtc::scoped_refptr<RTCStatsCollectorCallback> callback));
  MOCK_METHOD2(GetStats,
               void(rtc::scoped_refptr<RtpReceiverInterface> selector,
                    rtc::scoped_refptr<RTCStatsCollectorCallback> callback));
  MOCK_METHOD0(ClearStatsCache, void());
  MOCK_METHOD2(
      CreateDataChannel,
      rtc::scoped_refptr<DataChannelInterface>(const std::string& label,
                                               const DataChannelInit* config));
  MOCK_CONST_METHOD0(local_description, const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(remote_description, const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(current_local_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(current_remote_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(pending_local_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(pending_remote_description,
                     const SessionDescriptionInterface*());
  MOCK_METHOD2(CreateOffer,
               void(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints));
  MOCK_METHOD2(CreateOffer,
               void(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options));
  MOCK_METHOD2(CreateAnswer,
               void(CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options));
  MOCK_METHOD2(CreateAnswer,
               void(CreateSessionDescriptionObserver* observer,
                    const MediaConstraintsInterface* constraints));
  MOCK_METHOD2(SetLocalDescription,
               void(SetSessionDescriptionObserver* observer,
                    SessionDescriptionInterface* desc));
  MOCK_METHOD2(SetRemoteDescription,
               void(SetSessionDescriptionObserver* observer,
                    SessionDescriptionInterface* desc));
  MOCK_METHOD2(
      SetRemoteDescription,
      void(std::unique_ptr<SessionDescriptionInterface> desc,
           rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer));
  MOCK_METHOD0(GetConfiguration, PeerConnectionInterface::RTCConfiguration());
  MOCK_METHOD2(SetConfiguration,
               bool(const PeerConnectionInterface::RTCConfiguration& config,
                    RTCError* error));
  MOCK_METHOD1(SetConfiguration,
               bool(const PeerConnectionInterface::RTCConfiguration& config));
  MOCK_METHOD1(AddIceCandidate, bool(const IceCandidateInterface* candidate));
  MOCK_METHOD1(RemoveIceCandidates,
               bool(const std::vector<cricket::Candidate>& candidates));
  MOCK_METHOD1(RegisterUMAObserver, void(UMAObserver* observer));
  MOCK_METHOD1(SetBitrate, RTCError(const BitrateSettings& bitrate));
  MOCK_METHOD1(SetBitrate,
               RTCError(const BitrateParameters& bitrate_parameters));
  MOCK_METHOD1(SetBitrateAllocationStrategy,
               void(std::unique_ptr<rtc::BitrateAllocationStrategy>
                        bitrate_allocation_strategy));
  MOCK_METHOD1(SetAudioPlayout, void(bool playout));
  MOCK_METHOD1(SetAudioRecording, void(bool recording));
  MOCK_METHOD0(signaling_state, SignalingState());
  MOCK_METHOD0(ice_connection_state, IceConnectionState());
  MOCK_METHOD0(ice_gathering_state, IceGatheringState());
  MOCK_METHOD2(StartRtcEventLog,
               bool(rtc::PlatformFile file, int64_t max_size_bytes));
  MOCK_METHOD2(StartRtcEventLog,
               bool(std::unique_ptr<RtcEventLogOutput> output,
                    int64_t output_period_ms));
  MOCK_METHOD0(StopRtcEventLog, void());
  MOCK_METHOD0(Close, void());

  // PeerConnectionInternal
  MOCK_CONST_METHOD0(network_thread, rtc::Thread*());
  MOCK_CONST_METHOD0(worker_thread, rtc::Thread*());
  MOCK_CONST_METHOD0(signaling_thread, rtc::Thread*());
  MOCK_CONST_METHOD0(session_id, std::string());
  MOCK_CONST_METHOD0(initial_offerer, bool());
  MOCK_CONST_METHOD0(GetTransceiversInternal,
                     std::vector<rtc::scoped_refptr<
                         RtpTransceiverProxyWithInternal<RtpTransceiver>>>());
  MOCK_METHOD2(GetLocalTrackIdBySsrc,
               bool(uint32_t ssrc, std::string* track_id));
  MOCK_METHOD2(GetRemoteTrackIdBySsrc,
               bool(uint32_t ssrc, std::string* track_id));
  MOCK_METHOD0(SignalDataChannelCreated, sigslot::signal1<DataChannel*>&());
  MOCK_CONST_METHOD0(rtp_data_channel, cricket::RtpDataChannel*());
  MOCK_CONST_METHOD0(sctp_data_channels,
                     std::vector<rtc::scoped_refptr<DataChannel>>());
  MOCK_CONST_METHOD0(sctp_content_name, absl::optional<std::string>());
  MOCK_CONST_METHOD0(sctp_transport_name, absl::optional<std::string>());
  MOCK_CONST_METHOD0(GetPooledCandidateStats, cricket::CandidateStatsList());

  typedef std::map<std::string, std::string> TransportNamesByMid;
  MOCK_CONST_METHOD0(GetTransportNamesByMid, TransportNamesByMid());

  typedef std::map<std::string, cricket::TransportStats> TransportStatsByNames;
  MOCK_METHOD1(
      GetTransportStatsByNames,
      TransportStatsByNames(const std::set<std::string>& transport_names));
  MOCK_METHOD0(GetCallStats, Call::Stats());
  MOCK_METHOD2(GetLocalCertificate,
               bool(const std::string& transport_name,
                    rtc::scoped_refptr<rtc::RTCCertificate>* certificate));
  MOCK_METHOD1(
      GetRemoteSSLCertChain,
      std::unique_ptr<rtc::SSLCertChain>(const std::string& transport_name));
  MOCK_CONST_METHOD1(IceRestartPending, bool(const std::string& content_name));
  MOCK_CONST_METHOD1(NeedsIceRestart, bool(const std::string& content_name));
  MOCK_METHOD2(GetSslRole,
               bool(const std::string& content_name, rtc::SSLRole* role));
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_PEERCONNECTION_H_
