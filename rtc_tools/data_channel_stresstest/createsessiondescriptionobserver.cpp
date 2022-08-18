#include <iostream>
#include "createsessiondescriptionobserver.h"
#include "peerconnection.h"

CreateSessionDescriptionObserver::CreateSessionDescriptionObserver(Peerconnection &parent) :
		parent(parent) {
}

void CreateSessionDescriptionObserver::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
	parent.on_success_csd(desc);
}

void CreateSessionDescriptionObserver::OnFailure(webrtc::RTCError error) {
	std::cout << "CreateSessionDescriptionObserver::OnFailure !" << std::endl << error.message() << std::endl;
}

