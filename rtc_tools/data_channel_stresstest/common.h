#pragma once
#include <string>

extern size_t gDataChannelBufferHighSize;
extern size_t gDataChannelBufferLowSize;
extern size_t gDataChannelChunkSize;

struct Ice {
	std::string candidate;
	std::string sdp_mid;
	int sdp_mline_index;
};

