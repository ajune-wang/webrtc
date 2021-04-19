/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/dcsctp_transport.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "media/base/media_channel.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/socket/dcsctp_socket.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/task_utils/pending_task_safety_flag.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {

enum class WebrtcPPID : dcsctp::PPID::UnderlyingType {
  kNone = 0,  // No protocol is specified.
  // https://www.rfc-editor.org/rfc/rfc8832.html#section-8.1
  kDCEP = 50,
  // https://www.rfc-editor.org/rfc/rfc8831.html#section-8
  kString = 51,
  kBinaryPartial = 52,  // Deprecated
  kBinary = 53,
  kStringPartial = 54,  // Deprecated
  kStringEmpty = 56,
  kBinaryEmpty = 57,
};

WebrtcPPID GetPPID(cricket::DataMessageType message_type, size_t size) {
  switch (message_type) {
    case cricket::DMT_CONTROL:
      return WebrtcPPID::kDCEP;
    case cricket::DMT_TEXT:
      return size > 0 ? WebrtcPPID::kString : WebrtcPPID::kStringEmpty;
    case cricket::DMT_BINARY:
      return size > 0 ? WebrtcPPID::kBinary : WebrtcPPID::kBinaryEmpty;
    default:
      RTC_NOTREACHED();
  }
  return WebrtcPPID::kNone;
}

absl::optional<cricket::DataMessageType> GetDataMessageType(dcsctp::PPID ppid) {
  switch (static_cast<WebrtcPPID>(ppid.value())) {
    case WebrtcPPID::kNone:
      return cricket::DMT_NONE;
    case WebrtcPPID::kDCEP:
      return cricket::DMT_CONTROL;
    case WebrtcPPID::kString:
    case WebrtcPPID::kStringPartial:
    case WebrtcPPID::kStringEmpty:
      return cricket::DMT_TEXT;
    case WebrtcPPID::kBinary:
    case WebrtcPPID::kBinaryPartial:
    case WebrtcPPID::kBinaryEmpty:
      return cricket::DMT_BINARY;
  }
  return absl::nullopt;
}

bool IsEmptyPPID(dcsctp::PPID ppid) {
  WebrtcPPID webrtc_ppid = static_cast<WebrtcPPID>(ppid.value());
  return webrtc_ppid == WebrtcPPID::kStringEmpty ||
         webrtc_ppid == WebrtcPPID::kBinaryEmpty;
}

class ThreadTimeout : public dcsctp::Timeout {
 public:
  ThreadTimeout(rtc::Thread* thread,
                std::function<void(dcsctp::TimeoutID)> handle_timer)
      : thread_(thread), handle_timer_(std::move(handle_timer)) {}
  ~ThreadTimeout() override { Stop(); }

  void Start(dcsctp::DurationMs duration,
             dcsctp::TimeoutID timeout_id) override {
    RTC_DCHECK_RUN_ON(thread_);
    RTC_LOG(LS_VERBOSE) << "Start timer=" << timeout_id.value()
                        << ", duration=" << duration.value();

    pending_task_safety_flag_ = PendingTaskSafetyFlag::Create();
    pending_task_safety_flag_->SetAlive();

    thread_->PostDelayedTask(ToQueuedTask(pending_task_safety_flag_,
                                          [timeout_id, this]() {
                                            RTC_LOG(LS_VERBOSE)
                                                << "Timer expired: "
                                                << timeout_id.value();
                                            handle_timer_(timeout_id);
                                          }),
                             duration.value());
  }
  void Stop() override {
    RTC_DCHECK_RUN_ON(thread_);

    if (pending_task_safety_flag_)
      pending_task_safety_flag_->SetNotAlive();
  }

  void Restart(dcsctp::DurationMs duration,
               dcsctp::TimeoutID timeout_id) override {
    RTC_LOG(LS_VERBOSE) << "Re-Start timer=" << timeout_id.value()
                        << ", duration=" << duration.value();
    Timeout::Restart(duration, timeout_id);
  }

 private:
  rtc::Thread* thread_;
  rtc::scoped_refptr<PendingTaskSafetyFlag> pending_task_safety_flag_;
  std::function<void(dcsctp::TimeoutID)> handle_timer_;
};

}  // namespace

DcsctpTransport::DcsctpTransport(rtc::Thread* network_thread,
                                 rtc::PacketTransportInternal* transport,
                                 Clock* clock)
    : network_thread_(network_thread),
      transport_(transport),
      clock_(clock),
      random_(clock_->TimeInMicroseconds()),
      transport_was_ever_writable_(transport ? transport->writable() : false) {
  RTC_DCHECK_RUN_ON(network_thread_);
  static int instance_count = 0;
  rtc::StringBuilder sb;
  sb << debug_name_ << instance_count++;
  debug_name_ = sb.Release();
  ConnectTransportSignals();
}

DcsctpTransport::~DcsctpTransport() {
  if (socket_) {
    socket_->Close();
  }
}

void DcsctpTransport::SetDtlsTransport(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  DisconnectTransportSignals();
  transport_ = transport;
  ConnectTransportSignals();
  if (!transport_was_ever_writable_ && transport && transport->writable()) {
    transport_was_ever_writable_ = true;
    // New transport is writable, now we can start the SCTP connection if Start
    // was called already.
    if (socket_) {
      RTC_DCHECK(!socket_);
      socket_->Connect();
    }
  }
}

bool DcsctpTransport::Start(int local_sctp_port,
                            int remote_sctp_port,
                            int max_message_size) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(max_message_size > 0);

  RTC_LOG(LS_INFO) << debug_name_ << "->Start(local=" << local_sctp_port
                   << ", remote=" << remote_sctp_port
                   << ", max_message_size=" << max_message_size << ")";

  options_.local_port = local_sctp_port;
  options_.remote_port = remote_sctp_port;
  options_.mtu = dcsctp::DcSctpOptions::kMaxSafeMTUSize;
  max_message_size_ = max_message_size;

  socket_ = std::make_unique<dcsctp::DcSctpSocket>(debug_name_, this, nullptr,
                                                   options_);

  if (transport_was_ever_writable_) {
    socket_->Connect();
  }

  return true;
}

bool DcsctpTransport::OpenStream(int sid) {
  RTC_LOG(LS_INFO) << debug_name_ << "->OpenStream(" << sid << ").";
  return true;
}

bool DcsctpTransport::ResetStream(int sid) {
  RTC_LOG(LS_INFO) << debug_name_ << "->ResetStream(" << sid << ").";
  dcsctp::StreamID streams[1] = {dcsctp::StreamID(static_cast<uint16_t>(sid))};
  socket_->ResetStreams(streams);
  return false;
}

bool DcsctpTransport::SendData(const cricket::SendDataParams& params,
                               const rtc::CopyOnWriteBuffer& payload,
                               cricket::SendDataResult* result) {
  RTC_DCHECK_RUN_ON(network_thread_);

  RTC_LOG(LS_INFO) << debug_name_ << "->SendData(sid=" << params.sid
                   << ", type=" << params.type << ", length=" << payload.size()
                   << ").";

  std::vector<uint8_t> message_payload(payload.cdata(),
                                       payload.cdata() + payload.size());
  if (message_payload.empty()) {
    // https://www.rfc-editor.org/rfc/rfc8831.html#section-6.6
    // SCTP does not support the sending of empty user messages. Therefore, if
    // an empty message has to be sent, the appropriate PPID (WebRTC String
    // Empty or WebRTC Binary Empty) is used, and the SCTP user message of one
    // zero byte is sent.
    message_payload.push_back('\0');
  }

  dcsctp::DcSctpMessage message(
      dcsctp::StreamID(static_cast<uint16_t>(params.sid)),
      dcsctp::PPID(static_cast<uint16_t>(GetPPID(params.type, payload.size()))),
      std::move(message_payload));

  dcsctp::SendOptions options;
  options.unordered = dcsctp::IsUnordered(!params.ordered);
  if (params.max_rtx_ms > 0)
    options.lifetime = dcsctp::DurationMs(params.max_rtx_ms);
  if (params.max_rtx_count > 0)
    options.max_retransmissions = static_cast<size_t>(params.max_rtx_count);

  ClearError();
  socket_->Send(std::move(message), options);

  if (last_error_ == dcsctp::ErrorKind::kNoError) {
    *result = cricket::SDR_SUCCESS;
  } else if (last_error_ == dcsctp::ErrorKind::kResourceExhaustion) {
    *result = cricket::SDR_BLOCK;
    ready_to_send_data_ = false;
  } else {
    *result = cricket::SDR_ERROR;
  }
  ClearError();

  return *result == cricket::SDR_SUCCESS;
}

bool DcsctpTransport::ReadyToSendData() {
  return ready_to_send_data_;
}

int DcsctpTransport::max_message_size() const {
  return max_message_size_;
}

absl::optional<int> DcsctpTransport::max_outbound_streams() const {
  // TODO(orphis): Get this magic number from the socket
  return 65535;
}

absl::optional<int> DcsctpTransport::max_inbound_streams() const {
  // TODO(orphis): Get this magic number from the socket
  return 65535;
}

void DcsctpTransport::set_debug_name_for_testing(const char* debug_name) {
  debug_name_ = debug_name;
}

void DcsctpTransport::SendPacket(rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (data.size() > (options_.mtu)) {
    RTC_LOG(LS_ERROR) << debug_name_
                      << "->SendPacket(...): "
                         "SCTP seems to have made a packet that is bigger "
                         "than its official MTU: "
                      << data.size() << " vs max of " << options_.mtu;
    return;
  }
  if (max_message_size_ > 0 && data.size() > max_message_size_) {
    RTC_LOG(LS_ERROR) << debug_name_
                      << "->SendPacket(...): "
                         "Trying to send packet bigger "
                         "than the max message size: "
                      << data.size() << " vs max of " << max_message_size_;
    return;
  }
  TRACE_EVENT0("webrtc", "DcsctpTransport::SendPacket");

  RTC_LOG(LS_INFO) << debug_name_ << "->SendPacket(length=" << data.size()
                   << ")";

  transport_->SendPacket(reinterpret_cast<const char*>(data.data()),
                         data.size(), rtc::PacketOptions(), 0);
}

std::unique_ptr<dcsctp::Timeout> DcsctpTransport::CreateTimeout() {
  return std::make_unique<ThreadTimeout>(network_thread_,
                                         [this](dcsctp::TimeoutID timeout_id) {
                                           socket_->HandleTimeout(timeout_id);
                                         });
}

dcsctp::TimeMs DcsctpTransport::TimeMillis() {
  return dcsctp::TimeMs(clock_->TimeInMilliseconds());
}

uint32_t DcsctpTransport::GetRandomInt(uint32_t low, uint32_t high) {
  return random_.Rand(low, high);
}

void DcsctpTransport::NotifyOutgoingMessageBufferEmpty() {
  RTC_LOG(LS_VERBOSE) << debug_name_ << "->NotifyOutgoingMessageBufferEmpty()";
}

void DcsctpTransport::OnMessageReceived(dcsctp::DcSctpMessage message) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << debug_name_
                   << "->OnMessageReceived(sid=" << message.stream_id().value()
                   << ", ppid=" << message.ppid().value()
                   << ", length=" << message.payload().size() << ").";
  cricket::ReceiveDataParams receive_data_params;
  receive_data_params.sid = message.stream_id().value();
  auto type = GetDataMessageType(message.ppid());
  if (!type.has_value()) {
    RTC_LOG(LS_ERROR) << "Received an unknown PPID " << message.ppid().value()
                      << " on an SCTP packet. Dropping.";
  }
  receive_data_params.type = *type;
  // No seq_num or timestamp available from dcSCTP
  receive_data_params.seq_num = 0;
  receive_data_params.timestamp = 0;
  receive_buffer_.Clear();
  if (!IsEmptyPPID(message.ppid()))
    receive_buffer_.AppendData(message.payload().data(),
                               message.payload().size());

  SignalDataReceived(receive_data_params, receive_buffer_);
}

void DcsctpTransport::ClearError() {
  last_error_ = dcsctp::ErrorKind::kNoError;
}

void DcsctpTransport::OnError(dcsctp::ErrorKind error,
                              absl::string_view message) {
  RTC_LOG(LS_ERROR) << debug_name_ << "->OnError(error=" << error
                    << ", message=" << message;
  last_error_ = error;
}

void DcsctpTransport::OnAborted(dcsctp::ErrorKind error,
                                absl::string_view message) {
  RTC_LOG(LS_INFO) << debug_name_ << "->OnAborted().";
  ready_to_send_data_ = false;
  socket_connected_ = false;
}

void DcsctpTransport::OnConnected() {
  RTC_LOG(LS_INFO) << debug_name_ << "->OnConnected().";
  ready_to_send_data_ = true;
  socket_connected_ = true;
  SignalReadyToSendData();
  SignalAssociationChangeCommunicationUp();
}

void DcsctpTransport::OnClosed() {
  RTC_LOG(LS_INFO) << debug_name_ << "->OnClosed().";
  socket_connected_ = false;
  ready_to_send_data_ = false;
}

void DcsctpTransport::OnConnectionRestarted() {
  RTC_LOG(LS_INFO) << debug_name_ << "->OnConnectionRestarted().";
}

void DcsctpTransport::OnStreamsResetFailed(
    rtc::ArrayView<const dcsctp::StreamID> outgoing_streams,
    absl::string_view reason) {
  RTC_NOTREACHED();
}

void DcsctpTransport::OnStreamsResetPerformed(
    rtc::ArrayView<const dcsctp::StreamID> outgoing_streams) {
  for (auto& stream_id : outgoing_streams) {
    RTC_LOG(LS_VERBOSE)
        << debug_name_
        << "->OnStreamsResetPerformed(...): Outgoing stream reset"
        << ", sid=" << stream_id.value();
    SignalClosingProcedureComplete(stream_id.value());
  }
}

void DcsctpTransport::OnIncomingStreamsReset(
    rtc::ArrayView<const dcsctp::StreamID> incoming_streams) {
  for (auto& stream_id : incoming_streams) {
    RTC_LOG(LS_VERBOSE)
        << debug_name_ << "->OnIncomingStreamsReset(...): Incoming stream reset"
        << ", sid=" << stream_id.value();
    SignalClosingProcedureStartedRemotely(stream_id.value());
  }
}

void DcsctpTransport::OnSentMessageExpired(dcsctp::StreamID stream_id,
                                           dcsctp::PPID ppid,
                                           bool unsent) {
  RTC_LOG(LS_VERBOSE) << debug_name_ << "->OnSentMessageExpired().";
  RTC_NOTREACHED();
}

void DcsctpTransport::ConnectTransportSignals() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!transport_) {
    return;
  }
  transport_->SignalWritableState.connect(
      this, &DcsctpTransport::OnTransportWritablestate);
  transport_->SignalReadPacket.connect(this,
                                       &DcsctpTransport::OnTransportReadPacket);
  transport_->SignalClosed.connect(this, &DcsctpTransport::OnTransportClosed);
}

void DcsctpTransport::DisconnectTransportSignals() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!transport_) {
    return;
  }
  transport_->SignalWritableState.disconnect(this);
  transport_->SignalReadPacket.disconnect(this);
  transport_->SignalClosed.disconnect(this);
}

void DcsctpTransport::OnTransportWritablestate(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK_EQ(transport_, transport);

  RTC_LOG(LS_INFO) << debug_name_ << "->OnTransportWritablestate(), writable="
                   << transport->writable();

  if (!transport_was_ever_writable_ && transport->writable()) {
    transport_was_ever_writable_ = true;
    if (socket_) {
      socket_->Connect();
    }
  }
}

void DcsctpTransport::OnTransportReadPacket(
    rtc::PacketTransportInternal* transport,
    const char* data,
    size_t length,
    const int64_t& /* packet_time_us */,
    int flags) {
  if (flags) {
    // We are only interested in SCTP packets.
    return;
  }

  RTC_LOG(LS_INFO) << debug_name_
                   << "->OnTransportReadPacket(), length=" << length;
  if (socket_) {
    socket_->ReceivePacket(rtc::ArrayView<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data), length));
  }
}

void DcsctpTransport::OnTransportClosed(
    rtc::PacketTransportInternal* transport) {
  RTC_LOG(LS_VERBOSE) << debug_name_ << "->OnTransportClosed().";
  SignalClosedAbruptly();
}

}  // namespace webrtc
