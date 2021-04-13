/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_CALLBACK_DEFERRER_H_
#define NET_DCSCTP_SOCKET_CALLBACK_DEFERRER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"

namespace dcsctp {

// Defers callbacks until they can be safely triggered.
//
// There are a lot of callbacks from the dcSCTP library to the client,
// such as when messages are received or streams are closed. When the client
// receives these callbacks, the client is expected to be able to call into the
// library - from within the callback. For example, sending a reply message when
// a certain SCTP message has been received, or to reconnect when the connection
// was closed for any reason. This means that the dcSCTP library must always be
// in a consistent and stable state when these callbacks are delivered, and to
// ensure that's the case, callbacks are not immediately delivered from where
// they originate, but instead queued (deferred) by this class. At the end of
// any public API method that may result in callbacks, they are triggered and
// then delivered.
//
// There are a number of exceptions, which is clearly annotated in the API.
class CallbackDeferrer : public DcSctpSocketCallbacks {
 public:
  explicit CallbackDeferrer(DcSctpSocketCallbacks* underlying)
      : underlying_(*underlying) {}

  void TriggerDeferred() {
    for (auto& cb : deferred_) {
      cb(underlying_);
    }
    deferred_.clear();

    for (auto& message : received_messages_) {
      underlying_.OnMessageReceived(std::move(message));
    }
    received_messages_.clear();
  }

  void SendPacket(rtc::ArrayView<const uint8_t> data) override {
    // Will not be deferred - call directly.
    underlying_.SendPacket(data);
  }

  std::unique_ptr<Timeout> CreateTimeout() override {
    // Will not be deferred - call directly.
    return underlying_.CreateTimeout();
  }

  TimeMs TimeMillis() override {
    // Will not be deferred - call directly.
    return underlying_.TimeMillis();
  }

  uint32_t GetRandomInt(uint32_t low, uint32_t high) override {
    // Will not be deferred - call directly.
    return underlying_.GetRandomInt(low, high);
  }

  void NotifyOutgoingMessageBufferEmpty() override {
    // Will not be deferred - call directly.
    underlying_.NotifyOutgoingMessageBufferEmpty();
  }

  void OnMessageReceived(DcSctpMessage message) override {
    // Special handling as std::function can't capture move-only types.
    received_messages_.emplace_back(std::move(message));
  }

  void OnError(ErrorKind error, absl::string_view message) override {
    deferred_.emplace_back(
        [error, message = std::string(message)](DcSctpSocketCallbacks& cb) {
          cb.OnError(error, message);
        });
  }

  void OnAborted(ErrorKind error, absl::string_view message) override {
    deferred_.emplace_back(
        [error, message = std::string(message)](DcSctpSocketCallbacks& cb) {
          cb.OnAborted(error, message);
        });
  }

  void OnConnected() override {
    deferred_.emplace_back([](DcSctpSocketCallbacks& cb) { cb.OnConnected(); });
  }

  void OnClosed() override {
    deferred_.emplace_back([](DcSctpSocketCallbacks& cb) { cb.OnClosed(); });
  }

  void OnConnectionRestarted() override {
    deferred_.emplace_back(
        [](DcSctpSocketCallbacks& cb) { cb.OnConnectionRestarted(); });
  }

  void OnStreamsResetFailed(rtc::ArrayView<const StreamID> outgoing_streams,
                            absl::string_view reason) override {
    deferred_.emplace_back(
        [streams = std::vector<StreamID>(outgoing_streams.begin(),
                                         outgoing_streams.end()),
         reason = std::string(reason)](DcSctpSocketCallbacks& cb) {
          cb.OnStreamsResetFailed(streams, reason);
        });
  }

  void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) override {
    deferred_.emplace_back(
        [streams = std::vector<StreamID>(outgoing_streams.begin(),
                                         outgoing_streams.end())](
            DcSctpSocketCallbacks& cb) {
          cb.OnStreamsResetPerformed(streams);
        });
  }

  void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) override {
    deferred_.emplace_back(
        [streams = std::vector<StreamID>(incoming_streams.begin(),
                                         incoming_streams.end())](
            DcSctpSocketCallbacks& cb) { cb.OnIncomingStreamsReset(streams); });
  }

  void OnSentMessageExpired(StreamID stream_id,
                            PPID ppid,
                            bool unsent) override {
    deferred_.emplace_back(
        [stream_id, ppid, unsent](DcSctpSocketCallbacks& cb) {
          cb.OnSentMessageExpired(stream_id, ppid, unsent);
        });
  }

 private:
  DcSctpSocketCallbacks& underlying_;

  std::vector<std::function<void(DcSctpSocketCallbacks& cb)>> deferred_;
  std::vector<DcSctpMessage> received_messages_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_SOCKET_CALLBACK_DEFERRER_H_
