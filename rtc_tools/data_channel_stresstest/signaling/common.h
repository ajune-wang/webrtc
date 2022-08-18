#pragma once

#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/timerfd.h>

#include <netdb.h>

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>

// using namespace std;

uint64_t GetTimeMicros();
#define GetTimeMillis() ((uint64_t)(GetTimeMicros()/1000))

#define STR(x) (((std::string)(x)).c_str())
#define MAP_HAS1(m,k) ((bool)((m).find((k))!=(m).end()))
#define MAP_HAS2(m,k1,k2) ((MAP_HAS1((m),(k1))==true)?MAP_HAS1((m)[(k1)],(k2)):false)
#define MAP_HAS3(m,k1,k2,k3) ((MAP_HAS1((m),(k1)))?MAP_HAS2((m)[(k1)],(k2),(k3)):false)
#define FOR_MAP(m,k,v,i) for(std::map< k , v >::iterator i=(m).begin();i!=(m).end();i++)
#define MAP_KEY(i) ((i)->first)
#define MAP_VAL(i) ((i)->second)
#define MAP_ERASE1(m,k) if(MAP_HAS1((m),(k))) (m).erase((k));
#define MAP_ERASE2(m,k1,k2) \
if(MAP_HAS1((m),(k1))){ \
    MAP_ERASE1((m)[(k1)],(k2)); \
    if((m)[(k1)].size()==0) \
        MAP_ERASE1((m),(k1)); \
}
#define MAP_ERASE3(m,k1,k2,k3) \
if(MAP_HAS1((m),(k1))){ \
    MAP_ERASE2((m)[(k1)],(k2),(k3)); \
    if((m)[(k1)].size()==0) \
        MAP_ERASE1((m),(k1)); \
}

#define FOR_VECTOR(v,i) for(uint32_t i=0;i<(v).size();i++)
#define FOR_VECTOR_ITERATOR(e,v,i) for(std::vector<e>::iterator i=(v).begin();i!=(v).end();i++)
#define FOR_VECTOR_WITH_START(v,i,s) for(uint32_t i=s;i<(v).size();i++)
#define ADD_VECTOR_END(v,i) (v).push_back((i))
#define ADD_VECTOR_BEGIN(v,i) (v).insert((v).begin(),(i))
#define VECTOR_VAL(i) (*(i))

#define EHTONS(x) htons(x)
#define ENTOHS(x) ntohs(x)
#define EHTONL(x) htonl(x)
#define ENTOHL(x) ntohl(x)
#define EHTONLL(x) htonl(x)
#define ENTOHLL(x) ntohl(x)
#define EHTONLP(pNetworkPointer,hostLongValue) (*((uint32_t*)(pNetworkPointer)) = EHTONL(hostLongValue))
#define ENTOHLP(pNetworkPointer) ENTOHL(*((uint32_t *)(pNetworkPointer)))

#define SOCKET_TOS_DSCP_EF 184
#define SOCKET_TYPE int
#define SOCKET_INVALID (-1)
#define SOCKET_IS_INVALID(sock) ((sock)<0)
#define SOCKET_IS_VALID(sock) ((sock)>=0)
#define SOCKET_CLOSE(fd) do{ if(SOCKET_IS_VALID(fd)) { shutdown(fd, SHUT_WR); close(fd); } fd=SOCKET_INVALID; } while(0)
#define SOCKET_LAST_ERROR			errno
#define SOCKET_ERROR_EINPROGRESS	EINPROGRESS
#define SOCKET_ERROR_EAGAIN			EAGAIN
#define SOCKET_ERROR_EWOULDBLOCK	EWOULDBLOCK
#define SOCKET_ERROR_ECONNRESET		ECONNRESET
#define SOCKET_ERROR_ENOBUFS		ENOBUFS
#define SOCKET_ERROR_EBADF			EBADF
#define SOCKET_ERROR_EINTR			EINTR


#define MIN_SOCK_BUF_SIZE (256 * 1024)
#define MAX_SOCK_BUF_SIZE (2 * 1024 * 1024)

bool setFdJoinIPv4Multicast(SOCKET_TYPE sock, std::string bindIp, uint16_t bindPort, const std::string &ssmIp);
bool setFdCloseOnExec(SOCKET_TYPE fd);
bool setFdNonBlock(SOCKET_TYPE fd);
bool setFdNoSIGPIPE(SOCKET_TYPE fd);
bool setFdKeepAlive(SOCKET_TYPE fd, bool isUdp);
bool setFdNoNagle(SOCKET_TYPE fd, bool isUdp);
bool setFdReuseAddress(SOCKET_TYPE fd);
bool setFdTTL(SOCKET_TYPE fd, int af, uint8_t ttl);
bool setFdMulticastTTL(SOCKET_TYPE fd, uint8_t ttl);
bool setIPTOS(SOCKET_TYPE fd, uint8_t tos, bool isIpV6);
bool setFdLinger(SOCKET_TYPE fd);
bool setFdSndRcvBuff(SOCKET_TYPE fd, uint32_t sendSize, uint32_t recvSize);
bool setFdMaxSndRcvBuff(SOCKET_TYPE fd, bool isUdp);
bool setFdMinSendBuff(SOCKET_TYPE fd, int size, bool isUdp);
bool setFdOptions(SOCKET_TYPE fd, bool isUdp);

#define SO_NOSIGPIPE 0

#define READ_FD read
#define WRITE_FD write

#define MAKE_TAG8(a,b,c,d,e,f,g,h) ((uint64_t)(((uint64_t)(a))<<56)|(((uint64_t)(b))<<48)|(((uint64_t)(c))<<40)|(((uint64_t)(d))<<32)|(((uint64_t)(e))<<24)|(((uint64_t)(f))<<16)|(((uint64_t)(g))<<8)|((uint64_t)(h)))
#define MAKE_TAG7(a,b,c,d,e,f,g) MAKE_TAG8(a,b,c,d,e,f,g,0)
#define MAKE_TAG6(a,b,c,d,e,f) MAKE_TAG7(a,b,c,d,e,f,0)
#define MAKE_TAG5(a,b,c,d,e) MAKE_TAG6(a,b,c,d,e,0)
#define MAKE_TAG4(a,b,c,d) MAKE_TAG5(a,b,c,d,0)
#define MAKE_TAG3(a,b,c) MAKE_TAG4(a,b,c,0)
#define MAKE_TAG2(a,b) MAKE_TAG3(a,b,0)
#define MAKE_TAG1(a) MAKE_TAG2(a,0)

#define TAG_KIND_OF(tag,kind) ((bool)(((tag)&getTagMask((kind)))==(kind)))
#define PT_TCP MAKE_TAG3('T','C','P')


std::string format(const char *pFormat, ...);
std::string tagToString(uint64_t tag);
