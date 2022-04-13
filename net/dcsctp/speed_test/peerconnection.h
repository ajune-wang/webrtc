#pragma once
#include <string>

#include "net/dcsctp/speed_test/createsessiondescriptionobserver.h"
#include "net/dcsctp/speed_test/datachannel.h"
#include "net/dcsctp/speed_test/setsessiondescriptionobserver.h"

struct Ice;
class CreateSessionDescriptionObserver;
class SetSessionDescriptionObserver;

class Peerconnection : public webrtc::PeerConnectionObserver {
 public:
  Peerconnection(bool offerer);
  Peerconnection(int id, bool offerer);
  void on_success_csd(webrtc::SessionDescriptionInterface* desc);

  // webrtc::PeerConnectionObserver interface
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;

  bool CreateDataChannel(const std::string label);
  void Close();

 public:
  uint32_t _id;

  std::map<std::string, std::shared_ptr<DataChannel>> _datachannels;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peer_connection;
  rtc::scoped_refptr<CreateSessionDescriptionObserver> _csdo;
  rtc::scoped_refptr<SetSessionDescriptionObserver> _ssdo;

  std::function<void(int id, const std::string&)> _on_sdp;
  std::function<void(int id, const Ice&)> _on_ice;
  std::function<void()> _on_accept_ice;

  bool _offerer;

 private:
  static uint32_t _idGenerator;
};
