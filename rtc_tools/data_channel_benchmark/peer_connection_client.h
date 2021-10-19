/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_
#define RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/signaling_interface.h"

namespace webrtc {

class PeerConnectionClient : public sigslot::has_slots<>,
                             public webrtc::PeerConnectionObserver {
 public:
  explicit PeerConnectionClient(webrtc::PeerConnectionFactoryInterface* factory,
                                webrtc::SignalingInterface* signaling);

  ~PeerConnectionClient() override;

  // Add media streams to the peer connection and send an offer.
  bool StartPeerConnection();

  // Whether the peer connection is connected to some ICE servers.
  bool IsConnected();

  // Disconnect from the call.
  void Disconnect();

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection() {
    return peer_connection_;
  }

  std::vector<rtc::scoped_refptr<webrtc::DataChannelInterface>>&
  dataChannels() {
    return data_channels_;
  }

  static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
  CreateDefaultFactory(rtc::Thread* signaling_thread);

 private:
  void AddIceCandidate(
      std::unique_ptr<webrtc::IceCandidateInterface> candidate);
  bool SetRemoteDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface> desc);

  // Initialize the PeerConnection.
  bool InitializePeerConnection(
      webrtc::PeerConnectionFactoryInterface* factory);
  void DeletePeerConnection();

  // PeerConnectionObserver implementation.

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " new state: " << new_state;
  }

  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

  void OnNegotiationNeededEvent(uint32_t event_id) override;

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " receiving? " << receiving;
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  std::vector<rtc::scoped_refptr<webrtc::DataChannelInterface>> data_channels_;
  webrtc::SignalingInterface* signaling_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PeerConnectionClient);
};

// Observer that watches the events on the PeerConnectionClient.
class PeerConnectionClientObserver {
  // PeerConnectionClient is ready to connect to the remote peer.
  virtual void OnReadyToConnect() = 0;

  // PeerConnectionClient is connected to a peer.
  virtual void OnConnected() = 0;

  // PeerConnectionClient is disconnected.
  virtual void OnDisconnected() = 0;

 protected:
  virtual ~PeerConnectionClientObserver() {}
};

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
CreateDefaultFactory();

}  // namespace webrtc

#endif  // RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_
