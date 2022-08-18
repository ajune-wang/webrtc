#pragma once
#include "api/create_peerconnection_factory.h"
class Peerconnection;

class CreateSessionDescriptionObserver: public webrtc::CreateSessionDescriptionObserver {
private:
	Peerconnection &parent;
public:
	CreateSessionDescriptionObserver(Peerconnection &parent);
	void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
	void OnFailure(webrtc::RTCError error) override;
};
