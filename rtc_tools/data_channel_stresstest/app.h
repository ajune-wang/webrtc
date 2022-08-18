#pragma once
#include <string>
#include <map>

#include "api/create_peerconnection_factory.h"
#include "rtc_base/thread.h"

#include "rtc_tools/data_channel_stresstest/signaling/signaling.h"
#include "common.h"
#include "peerconnection.h"

class App {
public:
	std::unique_ptr<rtc::Thread> _network_thread;
	std::unique_ptr<rtc::Thread> _worker_thread;
	std::unique_ptr<rtc::Thread> _signaling_thread;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _peer_connection_factory;

	std::map<int, std::shared_ptr<Peerconnection>> _connections;

	webrtc::PeerConnectionInterface::RTCConfiguration _configuration;

	bool _offerer;
	signaling _signaling;


	void on_ice(int id, const Ice &ice);
	void on_sdp(int id, const std::string &sdp);
	void on_accept_ice();

public:
	App(std::string address, uint16_t port, bool offerer);

	void Init();

	void CreateOffer(int id);
	void OnOffer(int id, const std::string &parameter);
	void OnAnswer(int id, const std::string &parameter);
	void OnICE(int id, const Ice &ice_it);

	bool Run();
	void Release();
};
