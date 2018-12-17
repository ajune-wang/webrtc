/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/socket_server_adapters.h"

#include <string>

#include "rtc_base/bytebuffer.h"
#include "rtc_base/checks.h"

namespace rtc {

AsyncProxyServerSocket::AsyncProxyServerSocket(AsyncSocket* socket,
                                               size_t buffer_size)
    : BufferedReadAdapter(socket, buffer_size) {}

AsyncProxyServerSocket::~AsyncProxyServerSocket() = default;

// This is a SSL v2 CLIENT_HELLO message.
static const uint8_t kSslClientHello[] = {
    0x80, 0x46,                                            // msg len
    0x01,                                                  // CLIENT_HELLO
    0x03, 0x01,                                            // SSL 3.1
    0x00, 0x2d,                                            // ciphersuite len
    0x00, 0x00,                                            // session id len
    0x00, 0x10,                                            // challenge len
    0x01, 0x00, 0x80, 0x03, 0x00, 0x80, 0x07, 0x00, 0xc0,  // ciphersuites
    0x06, 0x00, 0x40, 0x02, 0x00, 0x80, 0x04, 0x00, 0x80,  //
    0x00, 0x00, 0x04, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x0a,  //
    0x00, 0xfe, 0xfe, 0x00, 0x00, 0x09, 0x00, 0x00, 0x64,  //
    0x00, 0x00, 0x62, 0x00, 0x00, 0x03, 0x00, 0x00, 0x06,  //
    0x1f, 0x17, 0x0c, 0xa6, 0x2f, 0x00, 0x78, 0xfc,        // challenge
    0x46, 0x55, 0x2e, 0xb1, 0x83, 0x39, 0xf1, 0xea         //
};

// This is a TLSv1 SERVER_HELLO message.
static const uint8_t kSslServerHello[] = {
    0x16,                                            // handshake message
    0x03, 0x01,                                      // SSL 3.1
    0x00, 0x4a,                                      // message len
    0x02,                                            // SERVER_HELLO
    0x00, 0x00, 0x46,                                // handshake len
    0x03, 0x01,                                      // SSL 3.1
    0x42, 0x85, 0x45, 0xa7, 0x27, 0xa9, 0x5d, 0xa0,  // server random
    0xb3, 0xc5, 0xe7, 0x53, 0xda, 0x48, 0x2b, 0x3f,  //
    0xc6, 0x5a, 0xca, 0x89, 0xc1, 0x58, 0x52, 0xa1,  //
    0x78, 0x3c, 0x5b, 0x17, 0x46, 0x00, 0x85, 0x3f,  //
    0x20,                                            // session id len
    0x0e, 0xd3, 0x06, 0x72, 0x5b, 0x5b, 0x1b, 0x5f,  // session id
    0x15, 0xac, 0x13, 0xf9, 0x88, 0x53, 0x9d, 0x9b,  //
    0xe8, 0x3d, 0x7b, 0x0c, 0x30, 0x32, 0x6e, 0x38,  //
    0x4d, 0xa2, 0x75, 0x57, 0x41, 0x6c, 0x34, 0x5c,  //
    0x00, 0x04,                                      // RSA/RC4-128/MD5
    0x00                                             // null compression
};

AsyncSSLServerSocket::AsyncSSLServerSocket(AsyncSocket* socket)
    : BufferedReadAdapter(socket, 1024) {
  BufferInput(true);
}

void AsyncSSLServerSocket::ProcessInput(char* data, size_t* len) {
  // We only accept client hello messages.
  if (*len < sizeof(kSslClientHello)) {
    return;
  }

  if (memcmp(kSslClientHello, data, sizeof(kSslClientHello)) != 0) {
    Close();
    SignalCloseEvent(this, 0);
    return;
  }

  *len -= sizeof(kSslClientHello);

  // Clients should not send more data until the handshake is completed.
  RTC_DCHECK(*len == 0);

  // Send a server hello back to the client.
  DirectSend(kSslServerHello, sizeof(kSslServerHello));

  // Handshake completed for us, redirect input to our parent.
  BufferInput(false);
}

AsyncSocksProxyServerSocket::AsyncSocksProxyServerSocket(AsyncSocket* socket)
    : AsyncProxyServerSocket(socket, kBufferSize), state_(SS_HELLO) {
  BufferInput(true);
}

void AsyncSocksProxyServerSocket::ProcessInput(char* data, size_t* len) {
  RTC_DCHECK(state_ < SS_CONNECT_PENDING);

  ByteBufferReader response(data, *len);
  if (state_ == SS_HELLO) {
    HandleHello(&response);
  } else if (state_ == SS_AUTH) {
    HandleAuth(&response);
  } else if (state_ == SS_CONNECT) {
    HandleConnect(&response);
  }

  // Consume parsed data
  *len = response.Length();
  memmove(data, response.Data(), *len);
}

void AsyncSocksProxyServerSocket::DirectSend(const ByteBufferWriter& buf) {
  BufferedReadAdapter::DirectSend(buf.Data(), buf.Length());
}

void AsyncSocksProxyServerSocket::HandleHello(ByteBufferReader* request) {
  uint8_t ver, num_methods;
  if (!request->ReadUInt8(&ver) || !request->ReadUInt8(&num_methods)) {
    Error(0);
    return;
  }

  if (ver != 5) {
    Error(0);
    return;
  }

  // Handle either no-auth (0) or user/pass auth (2)
  uint8_t method = 0xFF;
  if (num_methods > 0 && !request->ReadUInt8(&method)) {
    Error(0);
    return;
  }

  SendHelloReply(method);
  if (method == 0) {
    state_ = SS_CONNECT;
  } else if (method == 2) {
    state_ = SS_AUTH;
  } else {
    state_ = SS_ERROR;
  }
}

void AsyncSocksProxyServerSocket::SendHelloReply(uint8_t method) {
  ByteBufferWriter response;
  response.WriteUInt8(5);       // Socks Version
  response.WriteUInt8(method);  // Auth method
  DirectSend(response);
}

void AsyncSocksProxyServerSocket::HandleAuth(ByteBufferReader* request) {
  uint8_t ver, user_len, pass_len;
  std::string user, pass;
  if (!request->ReadUInt8(&ver) || !request->ReadUInt8(&user_len) ||
      !request->ReadString(&user, user_len) || !request->ReadUInt8(&pass_len) ||
      !request->ReadString(&pass, pass_len)) {
    Error(0);
    return;
  }

  SendAuthReply(0);
  state_ = SS_CONNECT;
}

void AsyncSocksProxyServerSocket::SendAuthReply(uint8_t result) {
  ByteBufferWriter response;
  response.WriteUInt8(1);  // Negotiation Version
  response.WriteUInt8(result);
  DirectSend(response);
}

void AsyncSocksProxyServerSocket::HandleConnect(ByteBufferReader* request) {
  uint8_t ver, command, reserved, addr_type;
  uint32_t ip;
  uint16_t port;
  if (!request->ReadUInt8(&ver) || !request->ReadUInt8(&command) ||
      !request->ReadUInt8(&reserved) || !request->ReadUInt8(&addr_type) ||
      !request->ReadUInt32(&ip) || !request->ReadUInt16(&port)) {
    Error(0);
    return;
  }

  if (ver != 5 || command != 1 || reserved != 0 || addr_type != 1) {
    Error(0);
    return;
  }

  SignalConnectRequest(this, SocketAddress(ip, port));
  state_ = SS_CONNECT_PENDING;
}

void AsyncSocksProxyServerSocket::SendConnectResult(int result,
                                                    const SocketAddress& addr) {
  if (state_ != SS_CONNECT_PENDING)
    return;

  ByteBufferWriter response;
  response.WriteUInt8(5);              // Socks version
  response.WriteUInt8((result != 0));  // 0x01 is generic error
  response.WriteUInt8(0);              // reserved
  response.WriteUInt8(1);              // IPv4 address
  response.WriteUInt32(addr.ip());
  response.WriteUInt16(addr.port());
  DirectSend(response);
  BufferInput(false);
  state_ = SS_TUNNEL;
}

void AsyncSocksProxyServerSocket::Error(int error) {
  state_ = SS_ERROR;
  BufferInput(false);
  Close();
  SetError(SOCKET_EACCES);
  SignalCloseEvent(this, error);
}

}  // namespace rtc
