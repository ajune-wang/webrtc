#include <iostream>
#include "api/create_peerconnection_factory.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"

#include <json/json.h>

#include "rtc_tools/data_channel_stresstest/signaling/signaling.h"
#include "common.h"
#include "app.h"

size_t gDataChannelBufferHighSize = (1 * 1024 * 1024);
size_t gDataChannelBufferLowSize = (500 * 1024);
size_t gDataChannelChunkSize = (4 * 1024);

App::App(std::string address, uint16_t port, bool offerer) :
		_offerer(offerer), _signaling(address, port, offerer) {

	_signaling.on_connect([&](int id) {
		std::cout << "########## CONNECTED [" << id << "] ############" << std::endl;
		if (_offerer) {
			CreateOffer(id);
		}
	});

	_signaling.on_message([&](int id, const std::string &message) {
		Json::Value v;
		std::istringstream { message } >> v;


		if (v.isMember("candidate")) {
			std::cout << "########### -> RECEIVED ICE CANDIDATE [" << id << "] ###########" << std::endl;
			Ice ice;
			ice.candidate = v["candidate"].asString();
			ice.sdp_mid = v["sdp_mid"].asString();
			ice.sdp_mline_index = v["sdp_mline_index"].asInt();
			OnICE(id, ice);
		}
		if (v.isMember("offer")) {
			std::cout << "########### -> RECEIVED OFFER [" << id << "] ###########" << std::endl;
			std::string sdp = v["offer"].asString();
			OnOffer(id, sdp);
		}

		if (v.isMember("answer")) {
			std::cout << "########### -> RECEIVED ANSWER [" << id << "] ###########" << std::endl;
			std::string answer = v["answer"].asString();
			OnAnswer(id, answer);
		}

	});

	_signaling.on_disconnect([&](int id) {
		std::cout << "########## DISCONNECTED  [" << id << "] ############" << std::endl;
		_connections[id]->_peer_connection->Close();
		_connections.erase(id);
	});

}

void App::on_sdp(int id, const std::string &sdp) {
	Json::Value sdp_o;
	sdp_o["id"] = id;
	if (_offerer) {
		std::cout << "########### SEND OFFER [" << id << "] -> ###########" << std::endl;
		sdp_o["offer"] = sdp;
	} else {
		std::cout << "########### SEND ANSWER [" << id << "] -> ###########" << std::endl;
		sdp_o["answer"] = sdp;
	}

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	std::string sdp_s = Json::writeString(builder, sdp_o);
	_signaling.send(id, sdp_s);
}

void App::on_ice(int id, const Ice &ice) {

	std::cout << "######### SEND ICE CANDIDATE [" << id << "] -> ###########" << std::endl;
	Json::Value ice_o;
	ice_o["id"] = id;
	ice_o["candidate"] = ice.candidate;
	ice_o["sdp_mid"] = ice.sdp_mid;
	ice_o["sdp_mline_index"] = ice.sdp_mline_index;

	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	std::string ice_s = Json::writeString(builder, ice_o);
	_signaling.send(id, ice_s);
}

void App::on_accept_ice() {
}

void App::Init() {
	std::cout << "#-> App::" << __func__ << std::endl;

	//WebRTC-DataChannel-Dcsctp/Disabled/
	webrtc::field_trial::InitFieldTrialsFromString("");

	rtc::InitializeSSL();

 	//rtc::LogMessage::LogToDebug(
    //  static_cast<rtc::LoggingSeverity>(rtc::LS_VERBOSE));
	//rtc::LogMessage::ConfigureLogging("thread tstamp info");

	webrtc::PeerConnectionInterface::IceServer ice_server;

	ice_server.uri = "stun:stun.l.google.com:19302";
	_configuration.servers.push_back(ice_server);

	_network_thread = rtc::Thread::CreateWithSocketServer();
	_network_thread->Start();

	_worker_thread = rtc::Thread::Create();
	_worker_thread->Start();

	_signaling_thread = rtc::Thread::Create();
	_signaling_thread->Start();

	webrtc::PeerConnectionFactoryDependencies dependencies;
	dependencies.network_thread = _network_thread.get();
	dependencies.worker_thread = _worker_thread.get();
	dependencies.signaling_thread = _signaling_thread.get();

	_peer_connection_factory = webrtc::CreateModularPeerConnectionFactory(std::move(dependencies));

	if (_peer_connection_factory.get() == nullptr) {
		std::cout << "Error on CreateModularPeerConnectionFactory." << std::endl;
		exit (EXIT_FAILURE);
	}

	_signaling.Init();

	std::cout << "<-# App::" << __func__ << std::endl;
}

bool App::Run() {

	return _signaling.Run();
}

void App::Release() {
	std::cout << "#-> App::" << __func__ << std::endl;

	for (auto const& [key, val] : _connections) {
		val->Close();
	}
	_connections.clear();

	_peer_connection_factory = nullptr;

	_network_thread->Stop();
	_worker_thread->Stop();
	_signaling_thread->Stop();

	rtc::CleanupSSL();
	std::cout << "<-# App::" << __func__ << std::endl;
}

void App::CreateOffer(int id) {

	std::cout << "#-> App::" << __func__ << std::endl;

	std::shared_ptr<Peerconnection> con(new Peerconnection(id, _offerer));

	con->_on_sdp = std::bind(&App::on_sdp, this, std::placeholders::_1, std::placeholders::_2);
	con->_on_ice = std::bind(&App::on_ice, this, std::placeholders::_1, std::placeholders::_2);
	con->_on_accept_ice = std::bind(&App::on_accept_ice, this);

	webrtc::PeerConnectionDependencies dependencies(con.get());

	auto result = _peer_connection_factory->CreatePeerConnectionOrError(_configuration, std::move(dependencies));
	if (!result.ok()) {
		std::cout << "Error on CreatePeerConnection." << std::endl;
		exit (EXIT_FAILURE);
	}

	con->_peer_connection = result.MoveValue();

	for (int i = 0; i < 10; i++) {
		con->CreateDataChannel();
	}

	con->_peer_connection->CreateOffer(con->_csdo.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

	_connections.insert(std::make_pair(con->_id, con));

	std::cout << "<-# App::" << __func__ << " [" << con->_id << "] " << std::endl;
}

void App::OnOffer(int id, const std::string &parameter) {

	std::cout << "#-> App::" << __func__ << " [" << id << "] " << std::endl;

	std::shared_ptr<Peerconnection> con(new Peerconnection(id, _offerer));

	con->_on_sdp = std::bind(&App::on_sdp, this, std::placeholders::_1, std::placeholders::_2);
	con->_on_ice = std::bind(&App::on_ice, this, std::placeholders::_1, std::placeholders::_2);
	con->_on_accept_ice = std::bind(&App::on_accept_ice, this);

	webrtc::PeerConnectionDependencies dependencies(con.get());

	auto result = _peer_connection_factory->CreatePeerConnectionOrError(_configuration, std::move(dependencies));
	if (!result.ok()) {
		std::cout << "Error on CreatePeerConnection." << std::endl;
		exit (EXIT_FAILURE);
	}

	con->_peer_connection = result.MoveValue();

	std::cout << "    App::" << __func__ << " id=" << id << " con->_id=" << con->_id << std::endl;

	_connections.insert(std::make_pair(con->_id, con));

	webrtc::SdpParseError error;
	webrtc::SessionDescriptionInterface *session_description(webrtc::CreateSessionDescription("offer", parameter, &error));

	if (session_description == nullptr) {
		std::cout << "Error on CreateSessionDescription." << std::endl << error.line << std::endl << error.description << std::endl;
		exit (EXIT_FAILURE);
	}

	_connections[con->_id]->_peer_connection->SetRemoteDescription(_connections[con->_id]->_ssdo.get(), session_description);
	_connections[con->_id]->_peer_connection->CreateAnswer(_connections[con->_id]->_csdo.get(), webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

	std::cout << "<-# App::" << __func__ << " [" << id << "] " << std::endl;
}

void App::OnAnswer(int id, const std::string &parameter) {

	std::cout << "#-> App::" << __func__ << " [" << id << "] " << std::endl;

	webrtc::SdpParseError error;
	webrtc::SessionDescriptionInterface *session_description(webrtc::CreateSessionDescription("answer", parameter, &error));
	if (session_description == nullptr) {
		std::cout << "Error on CreateSessionDescription." << std::endl << error.line << std::endl << error.description << std::endl;
		std::cout << "Answer SDP:begin" << std::endl << parameter << std::endl << "Answer SDP:end" << std::endl;
		exit (EXIT_FAILURE);
	}

	_connections[id]->_peer_connection->SetRemoteDescription(_connections[id]->_ssdo.get(), session_description);

	std::cout << "<-# App::" << __func__ << " [" << id << "] " << std::endl;
}

void App::OnICE(int id, const Ice &ice_it) {

	std::cout << "#-> App::" << __func__ << " [" << id << "] " << std::endl;

	webrtc::SdpParseError err_sdp;
	webrtc::IceCandidateInterface *ice = CreateIceCandidate(ice_it.sdp_mid, ice_it.sdp_mline_index, ice_it.candidate, &err_sdp);
	if (ice == NULL || (!err_sdp.line.empty() && !err_sdp.description.empty())) {
		std::cout << "Error on CreateIceCandidate" << std::endl << err_sdp.line << std::endl << err_sdp.description << std::endl;
		exit (EXIT_FAILURE);
	}

	_connections[id]->_peer_connection->AddIceCandidate(ice);

	std::cout << "<-# App::" << __func__ << " [" << id << "] " << std::endl;
}

