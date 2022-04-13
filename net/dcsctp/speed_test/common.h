#pragma once
#include <string>

#include "net/dcsctp/speed_test/datachannel.h"
#include "net/dcsctp/speed_test/peerconnection.h"

extern size_t gDataChannelBufferHighSize;
extern size_t gDataChannelBufferLowSize;
extern size_t gDataChannelChunkSize;

struct Ice {
  std::string candidate;
  std::string sdp_mid;
  int sdp_mline_index;
};
