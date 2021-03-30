#ifndef NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_H_
#define NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_H_

#include <stdint.h>

#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/packet_observer.h"
#include "net/dcsctp/public/timeout.h"

namespace dcsctp {

// Send options for sending messages
struct SendOptions {
  // If the message should be sent with unordered message delivery.
  bool unordered = false;

  // If positive, will discard messages that haven't been correctly sent and
  // received before the lifetime has expired. This is only available if the
  // peer supports Partial Reliability Extension (RFC3758).
  int lifetime_ms = -1;

  // If positive, limits the number of retransmissions. This is only available
  // if the peer supports Partial Reliability Extension (RFC3758).
  int max_retransmissions = -1;
};

enum class ErrorKind {
  // Indicates that no error has occurred. This will never be the case when
  // `OnError` or `OnAborted` is called.
  kNoError,
  // There have been too many retries or timeouts, and the library has given up.
  kTooManyRetries,
  // A command was received that is only possible to execute when the socket is
  // connected, which it is not.
  kNotConnected,
  // Parsing of the command or its parameters failed.
  kParseFailed,
  // Commands are received in the wrong sequence, which indicates a
  // synchronisation mismatch between the peers.
  kWrongSequence,
  // The peer has reported an issue using ERROR or ABORT command.
  kPeerReported,
  // The peer has performed a protocol violation.
  kProtocolViolation,
  // The receive or send buffers have been exhausted.
  kResourceExhaustion,
};

inline constexpr absl::string_view ToString(ErrorKind error) {
  switch (error) {
    case ErrorKind::kNoError:
      return "NO_ERROR";
    case ErrorKind::kTooManyRetries:
      return "TOO_MANY_RETRIES";
    case ErrorKind::kNotConnected:
      return "NOT_CONNECTED";
    case ErrorKind::kParseFailed:
      return "PARSE_FAILED";
    case ErrorKind::kWrongSequence:
      return "WRONG_SEQUENCE";
    case ErrorKind::kPeerReported:
      return "PEER_REPORTED";
    case ErrorKind::kProtocolViolation:
      return "PROTOCOL_VIOLATION";
    case ErrorKind::kResourceExhaustion:
      return "RESOURCE_EXHAUSTION";
  }
}

// Callbacks that the DcSctpSocket will be done synchronously to the owning
// client. Except for `SendPacket`, the client is allowed to call into
// the library from within a callback, within reason. The library is guaranteed
// to be in a correct and stable state when these callbacks are triggered.
//
// Theses callbacks are only synchronously triggered as a result of the client
// calling a public method in `DcSctpSocketInterface`.
class DcSctpSocketCallbacks {
 public:
  virtual ~DcSctpSocketCallbacks() = default;

  // Called when the library wants the packet serialized as `data` to be sent.
  // Note that it's not allowed to call into this library from within this
  // callback.
  virtual void SendPacket(rtc::ArrayView<const uint8_t> data) = 0;

  // Called when the library wants to create a Timeout. The callback must return
  // an object that implements that interface.
  virtual std::unique_ptr<Timeout> CreateTimeout() = 0;

  // Returns the current time in milliseconds (from any epoch).
  virtual int64_t TimeMillis() = 0;

  // Called when the library needs a random number uniformly distributed between
  // `low` (inclusive) and `high` (exclusive). The random number used by the
  // library are not used for cryptographic purposes there are no requirements
  // on a secure random number generator.
  virtual uint32_t GetRandomInt(uint32_t low, uint32_t high) = 0;

  // Called when the library has received an SCTP message in full and delivers
  // it to the upper layer.
  virtual void OnMessageReceived(DcSctpMessage message) = 0;

  // Triggered when an non-fatal error is reported by either this library or
  // from the other peer (by sending an ERROR command). These should be logged,
  // but no other action need to be taken as the association is still viable.
  virtual void OnError(ErrorKind error, absl::string_view message) = 0;

  // Triggered when the socket has aborted - either as decided by this socket
  // due to e.g. too many retransmission attempts, or by the peer when
  // receiving an ABORT command. No other callbacks will be done after this
  // callback, unless reconnecting.
  virtual void OnAborted(ErrorKind error, absl::string_view message) = 0;

  // Called when calling `Connect` succeeds, but also for incoming successful
  // connection attempts.
  virtual void OnConnected() = 0;

  // Called when the socket is closed in a controlled way. No other
  // callbacks will be done after this callback, unless reconnecting.
  virtual void OnClosed() = 0;

  // On connection restarted (by peer). This is just a notification, and the
  // association is expected to work fine after this call, but there could have
  // been packet loss as a result of restarting the association.
  virtual void OnConnectionRestarted() = 0;

  // Indicates that a stream reset request has failed.
  virtual void OnStreamsResetFailed(
      rtc::ArrayView<const uint16_t> outgoing_streams,
      absl::string_view reason) = 0;

  // Indicates that a stream reset request has been performed.
  virtual void OnStreamsResetPerformed(
      rtc::ArrayView<const uint16_t> outgoing_streams) = 0;

  // When a peer has reset some of its outgoing streams, this will be called. An
  // empty list indicates that all streams have been reset.
  virtual void OnIncomingStreamsReset(
      rtc::ArrayView<const uint16_t> incoming_streams) = 0;

  // If an outgoing message has expired before being completely sent.
  // TODO(boivie) Add some kind of message identifier.
  // TODO(boivie) Add callbacks for OnMessageSent and OnSentMessageAcked
  virtual void OnSentMessageExpired(uint16_t stream_id,
                                    uint32_t ppid,
                                    bool unsent) = 0;

  // Triggered when the outgoing message buffer is empty, meaning that there are
  // no more queued messages, but there can still be packets in-flight or to be
  // retransmitted. (in contrast to SCTP_SENDER_DRY_EVENT).
  // TODO(boivie): This is currently only used in benchmarks to have a steady
  // flow of packets to send, but it can add value if would send callbacks per
  // stream.
  virtual void OnOutgoingMessageBufferEmpty() = 0;
};

// The DcSctpSocket implementation implements the following interface.
class DcSctpSocketInterface {
 public:
  virtual ~DcSctpSocketInterface() = default;

  // To be called when an incoming SCTP packet is to be processed.
  virtual void ReceivePacket(rtc::ArrayView<const uint8_t> data) = 0;

  // To be called when a timeout has expired. The `timeout_id` is provided
  // when the timeout was initiated.
  virtual void HandleTimeout(uint64_t timeout_id) = 0;

  // Connects the socket. This is an asynchronous operation, and
  // `DcSctpSocketCallbacks::OnConnected` will be called on success.
  virtual void Connect() = 0;

  // Gracefully shutdowns the socket and sends all outstanding data. This is an
  // asynchronous operation and `ScSctpSocketCallbacks::OnClosed` will be called
  // on success.
  virtual void Shutdown() = 0;

  // Closes the connection non-gracefully. Will send ABORT if the connection is
  // not already closed. No callbacks will be made after Close() has returned.
  virtual void Close() = 0;

  // Sets a packet observer, which will be called on sent and received packets.
  virtual void SetPacketObserver(PacketObserver* observer) = 0;

  // Resetting streams is an asynchronous operation and the results will
  // be notified using `DcSctpSocketCallbacks::OnStreamsResetDone()`.
  // Note that only outgoing streams can be reset. When it's known that the
  // peer has reset its own outgoing streams,
  // `DcSctpSocketCallbacks::OnIncomingStreamReset` is called.
  //
  // Note that resetting a stream will also remove all queued messages on those
  // streams, but will ensure that the currently sent message (if any) is fully
  // sent before closing the stream.
  //
  // Resetting streams can only be done on established associations. Calling
  // this method on e.g. a closed association will not perform any operation.
  virtual void ResetStreams(
      rtc::ArrayView<const uint16_t> outgoing_streams) = 0;

  // Indicates if the peer supports resetting streams (RFC6525). If it's not yet
  // known, because the socket isn't properly connected, absl::nullopt will be
  // returned.
  virtual absl::optional<bool> SupportsStreamReset() const = 0;

  // Sends the message `message` using the provided send options.
  // Sending a message is an asynchrous operation, and the `OnError` callback
  // may be invoked to indicate any errors in sending the message.
  //
  // The association does not have to be established before calling this method.
  // If it's called before there is an established association, the message will
  // be queued.
  void Send(DcSctpMessage message, const SendOptions& send_options = {}) {
    SendMessage(std::move(message), send_options);
  }

 private:
  virtual void SendMessage(DcSctpMessage message,
                           const SendOptions& send_options) = 0;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_DCSCTP_SOCKET_H_
