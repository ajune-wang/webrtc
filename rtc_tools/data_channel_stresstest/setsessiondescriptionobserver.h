#pragma once
#include "api/create_peerconnection_factory.h"
class Peerconnection;
class SetSessionDescriptionObserver: public webrtc::SetSessionDescriptionObserver {
private:
	Peerconnection &_parent;
public:
	SetSessionDescriptionObserver(Peerconnection &parent);
	void OnSuccess() override;
	void OnFailure(webrtc::RTCError error) override;
};

