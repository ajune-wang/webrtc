all: rtcdc

# Change to your location
WEBRTC_DIR:=/usr/local/google/home/boivie/r/webrtc/src

CXX:=clang++
CFLAGS:=-g \
	-D_DEBUG \
	-DWEBRTC_HAVE_SCTP \
	-DWEBRTC_POSIX \
	-DWEBRTC_LINUX \
	-std=c++17 \
	-fPIC
#	-pedantic-errors -Wall -Wextra -Wno-unused-variable

#LDFLAGS= -fuse-ld=lld \
LDFLAGS=-Werror \
	-fPIC \
	-m64 \
	-pie \
	-g

INC:=-I. \
	-I$(WEBRTC_DIR)/ \
	-I$(WEBRTC_DIR)/third_party/abseil-cpp \
	-I$(WEBRTC_DIR)/build/linux/debian_sid_amd64-sysroot/usr/include/jsoncpp

LIB:=-ldl -lpthread -lrt -lgcc -lc -latomic

OBJ:=main.o \
	peerconnection.o \
	createsessiondescriptionobserver.o \
	datachannel.o \
	setsessiondescriptionobserver.o \
	app.o \
	signaling/signaling.o \
	signaling/common.o \
	signaling/iotimer.o \
	signaling/iohandler.o \
	signaling/iohandlermanager.o \
	signaling/iobuffer.o \
	signaling/tcpacceptor.o \
	signaling/tcpcarrier.o \
	signaling/socketaddress.o \
	signaling/baseprotocol.o \
	signaling/tcpprotocol.o

rtcdc: $(OBJ)
	clang++ $(LDFLAGS) $(WEBRTC_DIR)/out/Default/obj/libwebrtc.a $(OBJ) $(LIB) -o rtcdc

.cpp.o:
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

clean:
	git clean -fdx

