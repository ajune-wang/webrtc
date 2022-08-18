#include <iostream>

#include "common.h"
#include "app.h"

int main(int argc, char *argv[]) {

	if (argc < 3) {
		std::cout << "Usage: rtdc {ip address} {o|a} [high limit] [low limit] [chunk size] [port]" << std::endl;
		return 0;
	}
	std::string address = argv[1];
	uint16_t port = 60000;
	std::string offerer_answerer = argv[2];

	if (argc > 3) {
		gDataChannelBufferHighSize = atol(argv[3]);
	}

	if (argc > 4) {
		gDataChannelBufferLowSize = atol(argv[4]);
	}

	if (argc > 5) {
		gDataChannelChunkSize = atol(argv[5]);
	}

	if (argc > 6) {
		port = atol(argv[6]);
	}

	std::cout << "Address: " << address << std::endl;
	std::cout << "Offerer/Answerer: " << offerer_answerer << std::endl;

	bool offerer = (offerer_answerer == "o");

	App app(address, port, offerer);
	app.Init();
	app.Run();
	app.Release();

	return 0;
}
