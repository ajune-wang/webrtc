/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <cstdint>
#include <deque>

#include "api/array_view.h"
#include "benchmark/benchmark.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/socket/dcsctp_socket.h"

namespace dcsctp {

class BenchmarkTimeout : public Timeout {
 public:
  void Start(DurationMs duration_ms, TimeoutID timeout_id) override {}
  void Stop() override {}
};

class BenchmarkCallbacks : public DcSctpSocketCallbacks {
 public:
  void SendPacket(rtc::ArrayView<const uint8_t> data) override {
    sent_packets_.emplace_back(std::vector<uint8_t>(data.begin(), data.end()));
  }
  std::unique_ptr<Timeout> CreateTimeout() override {
    return std::make_unique<BenchmarkTimeout>();
  }
  TimeMs TimeMillis() override { return TimeMs(42); }
  uint32_t GetRandomInt(uint32_t low, uint32_t high) override {
    return low + 1;
  }
  void OnMessageReceived(DcSctpMessage message) override {
    received_messages_.emplace_back(std::move(message));
  }
  void OnError(ErrorKind error, absl::string_view message) override {}
  void OnAborted(ErrorKind error, absl::string_view message) override {}
  void OnConnected() override {}
  void OnClosed() override {}
  void OnConnectionRestarted() override {}
  void OnStreamsResetFailed(rtc::ArrayView<const StreamID> outgoing_streams,
                            absl::string_view reason) override {}
  void OnStreamsResetPerformed(
      rtc::ArrayView<const StreamID> outgoing_streams) override {}
  void OnIncomingStreamsReset(
      rtc::ArrayView<const StreamID> incoming_streams) override {}

  void NotifyOutgoingMessageBufferEmpty() override {
    is_outgoing_message_buffer_empty_ = true;
  }

  bool IsOutgoingMessageBufferEmpty() {
    if (is_outgoing_message_buffer_empty_) {
      is_outgoing_message_buffer_empty_ = false;
      return true;
    }
    return false;
  }

  bool HasPacket() const { return !sent_packets_.empty(); }

  bool HasMessage() const { return !received_messages_.empty(); }

  std::vector<uint8_t> ConsumeSentPacket() {
    if (sent_packets_.empty()) {
      return {};
    }
    std::vector<uint8_t> ret = sent_packets_.front();
    sent_packets_.pop_front();
    return ret;
  }
  absl::optional<DcSctpMessage> ConsumeReceivedMessage() {
    if (received_messages_.empty()) {
      return absl::nullopt;
    }
    DcSctpMessage ret = std::move(received_messages_.front());
    received_messages_.pop_front();
    return ret;
  }

 private:
  bool is_outgoing_message_buffer_empty_ = false;
  std::deque<std::vector<uint8_t>> sent_packets_;
  std::deque<DcSctpMessage> received_messages_;
};

// Sets up two socket with one continuously sending data to the other one.
void BM_PumpData(benchmark::State& state) {
  const DcSctpOptions options;
  BenchmarkCallbacks cb_a;
  BenchmarkCallbacks cb_z;

  DcSctpSocket sock_a("A", &cb_a, nullptr, options);
  DcSctpSocket sock_z("Z", &cb_z, nullptr, options);
  size_t message_size = state.range(0);

  sock_a.Connect();

  // Initiate the sending. Remaining packets will be sent on the
  // NotifyOutgoingMessageBufferEmpty callback.
  SendOptions opts;
  opts.unordered = IsUnordered(true);

  sock_a.Send(
      DcSctpMessage(StreamID(1), PPID(53), std::vector<uint8_t>(message_size)),
      opts);

  size_t num_sent = 0;
  for (auto _ : state) {
    while (cb_a.HasPacket() || cb_z.HasPacket()) {
      sock_z.ReceivePacket(cb_a.ConsumeSentPacket());
      sock_a.ReceivePacket(cb_z.ConsumeSentPacket());
    }

    if (cb_a.IsOutgoingMessageBufferEmpty()) {
      // Don't want to send from within the OutgoingMessageBufferEmpty callback,
      // as we will get a forever-growing stack.
      sock_a.Send(DcSctpMessage(StreamID(1), PPID(53),
                                std::vector<uint8_t>(message_size)),
                  opts);
      ++num_sent;
    }
    // Ensure that we don't queue too many messages.
    cb_z.ConsumeReceivedMessage();
  }
  state.SetBytesProcessed(num_sent * message_size);
  state.SetItemsProcessed(num_sent);
}
BENCHMARK(BM_PumpData)
    ->Arg(1)
    ->Arg(8)
    ->Arg(128)
    ->Arg(512)
    ->Arg(1024)
    ->Arg(2048)
    ->Arg(4096)
    ->Arg(8192)
    ->Arg(65536);

// Sets up two socket with one sending a message to the other one, which
// replies.
void BM_EchoServer(benchmark::State& state) {
  const DcSctpOptions options;
  BenchmarkCallbacks cb_a;
  BenchmarkCallbacks cb_z;

  DcSctpSocket sock_a("A", &cb_a, nullptr, options);
  DcSctpSocket sock_z("Z", &cb_z, nullptr, options);
  size_t message_size = state.range(0);

  sock_a.Connect();

  // Initiate the sending. Remaining packets will be sent when the message is
  // received on the other side.
  SendOptions opts;
  opts.unordered = IsUnordered(true);

  sock_a.Send(
      DcSctpMessage(StreamID(1), PPID(53), std::vector<uint8_t>(message_size)),
      opts);

  size_t num_sent = 0;
  for (auto _ : state) {
    while (cb_a.HasPacket() || cb_z.HasPacket()) {
      sock_z.ReceivePacket(cb_a.ConsumeSentPacket());
      sock_a.ReceivePacket(cb_z.ConsumeSentPacket());
    }

    if (cb_a.HasMessage()) {
      sock_a.Send(*std::move(cb_a.ConsumeReceivedMessage()), opts);
      num_sent++;
    }
    if (cb_z.HasMessage()) {
      sock_z.Send(*std::move(cb_z.ConsumeReceivedMessage()), opts);
      num_sent++;
    }
  }
  state.SetBytesProcessed(num_sent * message_size);
  state.SetItemsProcessed(num_sent);
}
BENCHMARK(BM_EchoServer)
    ->Arg(1)
    ->Arg(8)
    ->Arg(128)
    ->Arg(512)
    ->Arg(1024)
    ->Arg(2048)
    ->Arg(4096)
    ->Arg(8192)
    ->Arg(65536);

}  // namespace dcsctp
