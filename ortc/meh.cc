#include "api/jsepicecandidate.h"

int main(int argc, char** argv) {
  webrtc::CreateIceCandidate("!!", 17, "18", nullptr);
  return 0;
}
