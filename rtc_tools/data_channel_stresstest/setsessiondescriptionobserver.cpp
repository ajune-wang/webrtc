#include <iostream>
#include "setsessiondescriptionobserver.h"
#include "peerconnection.h"

SetSessionDescriptionObserver::SetSessionDescriptionObserver(Peerconnection &parent) :
		_parent(parent) {
}

void SetSessionDescriptionObserver::OnSuccess() {
	std::cout << "SetSessionDescriptionObserver::OnSuccess" << std::endl;
	if (_parent._on_accept_ice) {
		_parent._on_accept_ice();
	}
}

void SetSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
	std::cout << "SetSessionDescriptionObserver::OnFailure" << std::endl << error.message() << std::endl;
}

