/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Data Channel Benchmarking tool.
 *
 *  Create a server using: ./data_channel_benchmark --server --port 12345
 *  Start the flow of data from the server to a client using:
 *  ./data_channel_benchmark --port 12345 --transfer_size 100 --packet_size 8196
 *  The throughput is reported on the server console.
 *
 *  The negotiation does not require a 3rd party server and is done over a gRPC
 *  transport. No TURN server is configured, so both peers need to be reachable
 *  using STUN only.
 */
#include <inttypes.h>

#include <charconv>

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "rtc_base/event.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/grpc_signaling.h"
#include "rtc_tools/data_channel_benchmark/peer_connection_client.h"
#include "system_wrappers/include/field_trial.h"

ABSL_FLAG(bool, server, false, "Server mode");
ABSL_FLAG(bool, oneshot, true, "Terminate after serving a client");
ABSL_FLAG(std::string, address, "localhost", "Connect to server address");
ABSL_FLAG(uint16_t, port, 0, "Connect to port (0 for random)");
ABSL_FLAG(uint64_t, transfer_size, 2, "Transfer size (MiB)");
ABSL_FLAG(uint64_t, packet_size, 256 * 1024, "Packet size");
ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

class DataChannelObserverImpl : public webrtc::DataChannelObserver {
 public:
  explicit DataChannelObserverImpl(webrtc::DataChannelInterface* dc)
      : dc_(dc), bytes_received_(0) {}
  void OnStateChange() override {
    RTC_LOG(LS_INFO) << "State changed to " << dc_->state();
    switch (dc_->state()) {
      case webrtc::DataChannelInterface::DataState::kOpen:
        open_event_.Set();
        break;
      case webrtc::DataChannelInterface::DataState::kClosed:
        closed_event_.Set();
        break;
      default:
        break;
    }
  }
  void OnMessage(const webrtc::DataBuffer& buffer) override {
    bytes_received_ += buffer.data.size();
    if (bytes_received_threshold_ &&
        bytes_received_ >= bytes_received_threshold_) {
      bytes_received_event_.Set();
    }

    if (first_message_.empty() && !buffer.binary) {
      first_message_.assign(buffer.data.cdata<char>(), buffer.data.size());
      first_message_event_.Set();
    }
  }
  void OnBufferedAmountChange(uint64_t sent_data_size) override {
    if (dc_->buffered_amount() <
        webrtc::DataChannelInterface::MaxSendQueueSize() / 2)
      low_buffered_threshold_event_.Set();
    else
      low_buffered_threshold_event_.Reset();
  }

  bool WaitForOpenState(int duration_ms) {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kOpen ||
           open_event_.Wait(duration_ms);
  }
  bool WaitForClosedState(int duration_ms) {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kClosed ||
           closed_event_.Wait(duration_ms);
  }

  // Set how many received bytes are required until
  // WaitForBytesReceivedThreshold return true.
  void SetBytesReceivedThreshold(uint64_t bytes_received_threshold) {
    bytes_received_threshold_ = bytes_received_threshold;
    if (bytes_received_ >= bytes_received_threshold_)
      bytes_received_event_.Set();
  }
  // Wait until the received byte count reaches the desired value.
  bool WaitForBytesReceivedThreshold(int duration_ms) {
    return (bytes_received_threshold_ &&
            bytes_received_ >= bytes_received_threshold_) ||
           bytes_received_event_.Wait(duration_ms);
  }

  bool WaitForLowbufferedThreshold(int duration_ms) {
    return low_buffered_threshold_event_.Wait(duration_ms);
  }
  std::string FirstMessage() { return first_message_; }
  bool WaitForFirstMessage(int duration_ms) {
    return first_message_event_.Wait(duration_ms);
  }

 private:
  webrtc::DataChannelInterface* dc_;
  rtc::Event open_event_;
  rtc::Event closed_event_;
  rtc::Event bytes_received_event_;
  absl::optional<uint64_t> bytes_received_threshold_;
  uint64_t bytes_received_;
  rtc::Event low_buffered_threshold_event_;
  std::string first_message_;
  rtc::Event first_message_event_;
};

int main(int argc, char** argv) {
  rtc::InitializeSSL();
  absl::ParseCommandLine(argc, argv);

  bool is_server = absl::GetFlag(FLAGS_server);
  bool oneshot = absl::GetFlag(FLAGS_oneshot);
  uint16_t port = absl::GetFlag(FLAGS_port);
  std::string server_address = absl::GetFlag(FLAGS_address);
  std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);

  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  auto signaling_thread = rtc::Thread::Create();
  signaling_thread->Start();

  if (is_server) {
    // Start server
    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());

    auto grpc_server = webrtc::GrpcSignalingServer::Create(
        [factory = rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>(
             factory)](webrtc::SignalingInterface* signaling) {
          webrtc::PeerConnectionClient client(factory.get(), signaling);
          client.StartPeerConnection();
          auto peer_connection = client.peerConnection();

          // Set up the data channel
          auto dc_or_error =
              peer_connection->CreateDataChannelOrError("benchmark", nullptr);
          auto data_channel = dc_or_error.MoveValue();
          auto data_channel_observer =
              std::make_unique<DataChannelObserverImpl>(data_channel);
          data_channel->RegisterObserver(data_channel_observer.get());
          absl::Cleanup unregister_observer(
              [data_channel] { data_channel->UnregisterObserver(); });

          // Wait for a first message from the remote peer.
          // It configures how much data should be sent and how big the packets
          // should be.
          // First message is "packet_size,transfer_size".
          data_channel_observer->WaitForFirstMessage(rtc::Event::kForever);
          auto first_message = data_channel_observer->FirstMessage();

          auto parameters = rtc::split(first_message, ',');
          uint64_t packet_size, transfer_size;
          std::from_chars(parameters[0].data(),
                          parameters[0].data() + parameters[0].size(),
                          packet_size, 10);
          std::from_chars(parameters[1].data(),
                          parameters[1].data() + parameters[1].size(),
                          transfer_size, 10);

          // Wait for the sender and receiver peers to stabilize
          absl::SleepFor(absl::Seconds(1));

          std::string data(packet_size, '0');
          int64_t remaining_data = (int64_t)transfer_size;

          auto begin_time = absl::Now();

          while (remaining_data > 0ll) {
            if (remaining_data < (int64_t)data.size())
              data.resize(remaining_data);

            rtc::CopyOnWriteBuffer buffer(data);
            webrtc::DataBuffer data_buffer(buffer, true);
            if (!data_channel->Send(data_buffer)) {
              // If the send() call failed, the buffers are full.
              // We wait until there's more room.
              data_channel_observer->WaitForLowbufferedThreshold(
                  rtc::Event::kForever);
              continue;
            }
            remaining_data -= buffer.size();
            fprintf(stderr,
                    "Progress: %" PRId64 " / %" PRId64 " (%" PRId64 "%%)\n",
                    (transfer_size - remaining_data), transfer_size,
                    (100 - remaining_data * 100 / transfer_size));
          }

          // Receiver signals the data channel close event when it has received
          // all the data it requested.
          data_channel_observer->WaitForClosedState(rtc::Event::kForever);

          auto end_time = absl::Now();
          auto duration_ms = absl::ToDoubleMilliseconds(end_time - begin_time);
          printf("Elapsed time: %gms %gMB/s\n", duration_ms,
                 ((static_cast<double>(transfer_size) / 1024 / 1024) /
                  (duration_ms / 1000)));
        },
        port, oneshot);
    grpc_server->Start();
    grpc_server->Wait();
  } else {
    uint64_t transfer_size = absl::GetFlag(FLAGS_transfer_size) * 1024 * 1024;
    uint64_t packet_size = absl::GetFlag(FLAGS_packet_size);

    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());
    auto grpc_client = webrtc::GrpcSignalingClient::Create(
        server_address + ":" + std::to_string(port));
    webrtc::PeerConnectionClient client(factory.get(),
                                        grpc_client->signaling_client());

    // Set up the callback to receive the data channel from the sender.
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
    rtc::Event got_data_channel;
    client.SetOnDataChannel(
        [&data_channel, &got_data_channel](
            rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
          data_channel = channel;
          got_data_channel.Set();
        });

    // Connect to the server.
    if (!grpc_client->Start()) {
      fprintf(stderr, "Failed to connect to server\n");
      return 1;
    }

    // Wait for the data channel to be received
    got_data_channel.Wait(rtc::Event::kForever);

    // DataChannel needs an observer to start draining the read queue
    DataChannelObserverImpl observer(data_channel.get());
    observer.SetBytesReceivedThreshold(transfer_size);
    data_channel->RegisterObserver(&observer);
    absl::Cleanup unregister_observer(
        [data_channel] { data_channel->UnregisterObserver(); });

    // Send a configuration string to the server to tell it to send
    // 'packet_size' bytes packets and send a total of 'transfer_size' MB.
    observer.WaitForOpenState(rtc::Event::kForever);
    char buffer[64];
    rtc::SimpleStringBuilder sb(buffer);
    sb << packet_size << "," << transfer_size;
    if (!data_channel->Send(webrtc::DataBuffer(std::string(sb.str())))) {
      fprintf(stderr, "Failed to send parameter string\n");
      return 1;
    }

    // Wait until we have received all the data
    observer.WaitForBytesReceivedThreshold(rtc::Event::kForever);

    // Close the data channel, signaling to the server we have received
    // all the requested data.
    data_channel->Close();
  }

  signaling_thread->Quit();

  return 0;
}
