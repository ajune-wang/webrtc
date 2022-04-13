#include "peerconnection.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

#include "api/create_peerconnection_factory.h"
#include "net/dcsctp/speed_test/common.h"

uint32_t Peerconnection::_idGenerator = rand() % 100000;

Peerconnection::Peerconnection(bool offerer)
    : _csdo(new rtc::RefCountedObject<CreateSessionDescriptionObserver>(
          *this)),  //
      _ssdo(
          new rtc::RefCountedObject<SetSessionDescriptionObserver>(*this)),  //
      _offerer(offerer) {
  _id = ++_idGenerator;
}

Peerconnection::Peerconnection(int id, bool offerer)
    : _csdo(new rtc::RefCountedObject<CreateSessionDescriptionObserver>(
          *this)),  //
      _ssdo(
          new rtc::RefCountedObject<SetSessionDescriptionObserver>(*this)),  //
      _offerer(offerer) {
  _id = id;
}

void Peerconnection::on_success_csd(webrtc::SessionDescriptionInterface* desc) {
  _peer_connection->SetLocalDescription(_ssdo, desc);

  std::string sdp;
  desc->ToString(&sdp);
  std::cout << sdp << std::endl;

  _on_sdp(_id, sdp);
}

void Peerconnection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  std::cout << "    PeerConnection::" << __func__ << "  " << new_state
            << std::endl;
}

void Peerconnection::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  std::cout << "#-> PeerConnection::" << __func__
            << " ####################### -> ON DATA CHANNEL "
            << data_channel->label() << " ################" << std::endl;

  std::shared_ptr<DataChannel> dc(new DataChannel(*this, data_channel));

  _datachannels.insert(std::make_pair(data_channel->label(), dc));

  std::cout << "<-# PeerConnection::" << __func__ << std::endl;
}

void Peerconnection::OnRenegotiationNeeded() {
  std::cout << "#-> PeerConnection::" << __func__ << std::endl;
  std::cout << "<-# PeerConnection::" << __func__ << std::endl;
}

void Peerconnection::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  std::cout << "    PeerConnection::" << __func__ << " " << new_state
            << std::endl;
}

void Peerconnection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  std::cout << "    PeerConnection::" << __func__ << " " << new_state
            << std::endl;
}

void Peerconnection::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  std::cout << "#-> PeerConnection::" << __func__ << std::endl;
  Ice ice;
  candidate->ToString(&ice.candidate);
  ice.sdp_mid = candidate->sdp_mid();
  ice.sdp_mline_index = candidate->sdp_mline_index();

  if (_on_ice)
    _on_ice(_id, ice);

  std::cout << "<-# PeerConnection::" << __func__ << std::endl;
}

bool Peerconnection::CreateDataChannel(const std::string label) {
  std::cout << "#-> PeerConnection::" << __func__
            << " ####################### CREATE DATA CHANNEL " << label
            << " ################" << std::endl;
  webrtc::DataChannelInit config;
  config.ordered = true;
  config.reliable = true;

  auto dc_result = _peer_connection->CreateDataChannelOrError(label, &config);
  if (!dc_result.ok()) {
    std::cout << "Error on CreateDataChannelOrError." << std::endl;
    return false;
  }

  std::shared_ptr<DataChannel> dc(
      new DataChannel(*this, dc_result.MoveValue()));
  _datachannels.insert(std::make_pair(label, dc));

  return false;
}

void Peerconnection::Close() {
  for (auto const& [key, val] : _datachannels) {
    val->Close();
  }
  _datachannels.clear();

  if (_peer_connection)
    _peer_connection->Close();

  _peer_connection = nullptr;
}
