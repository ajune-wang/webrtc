/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/datagram_dtls_adaptor.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_transport_state.h"
#include "logging/rtc_event_log/events/rtc_event_dtls_writable_state.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

WEBRTC_DEFINE_bool(bypass_datagram_dtls_test_only,
                   false,
                   "If true send unencrypted packets directly to ICE, "
                   "bypassing datagtram transport. Use in tests only.");

namespace cricket {

DatagramDtlsAdaptor::DatagramDtlsAdaptor(
    std::unique_ptr<IceTransportInternal> ice_transport,
    std::unique_ptr<webrtc::DatagramTransportInterface> datagram_transport,
    const webrtc::CryptoOptions& crypto_options)
    : crypto_options_(webrtc::CryptoOptions::NoGcm()),
      ice_transport_(std::move(ice_transport)),
      datagram_transport_(std::move(datagram_transport)) {
  RTC_DCHECK(ice_transport_);
  RTC_DCHECK(datagram_transport_);
  ConnectToIceTransport();
}

void DatagramDtlsAdaptor::ConnectToIceTransport() {
  if (FLAG_bypass_datagram_dtls_test_only) {
    // In bypass mode we have to subscribe to ICE read and sent events.
    // Test only case to use ICE directly instead of data transport.
    ice_transport_->SignalReadPacket.connect(
        this, &DatagramDtlsAdaptor::OnReadPacket);

    ice_transport_->SignalSentPacket.connect(
        this, &DatagramDtlsAdaptor::OnSentPacket);

    ice_transport_->SignalWritableState.connect(
        this, &DatagramDtlsAdaptor::OnWritableState);
    ice_transport_->SignalReadyToSend.connect(
        this, &DatagramDtlsAdaptor::OnReadyToSend);
    ice_transport_->SignalReceivingState.connect(
        this, &DatagramDtlsAdaptor::OnReceivingState);
  } else {
    // Subscribe to Data Transport read packets.
    datagram_transport_->SetDatagramSink(this);
    datagram_transport_->SetTransportStateCallback(this);

    // Datagram transport does not propagate network route change.
    ice_transport_->SignalNetworkRouteChanged.connect(
        this, &DatagramDtlsAdaptor::OnNetworkRouteChanged);
  }
}

DatagramDtlsAdaptor::~DatagramDtlsAdaptor() {
  // Unsubscribe from Datagram Transport dinks.
  datagram_transport_->SetDatagramSink(nullptr);
  datagram_transport_->SetTransportStateCallback(nullptr);

  // Make sure datagram transport is destroyed before ICE.
  datagram_transport_.reset();
  ice_transport_.reset();
}

const webrtc::CryptoOptions& DatagramDtlsAdaptor::crypto_options() const {
  return crypto_options_;
}

int DatagramDtlsAdaptor::SendPacket(const char* data,
                                    size_t len,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  // TODO(sukhanov): Handle options and flags.
  if (FLAG_bypass_datagram_dtls_test_only) {
    // In bypass mode sent directly to ICE.
    return ice_transport_->SendPacket(data, len, options);
  }

  // Send datagram with id equal to options.packet_id, so we get it back
  // in DatagramDtlsAdaptor::OnDatagramSent() and propagate notification
  // up.
  webrtc::RTCError error = datagram_transport_->SendDatagram(
      rtc::ArrayView<const uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                    len),
      /*datagram_id=*/options.packet_id);

  return (error.ok() ? len : -1);
}

void DatagramDtlsAdaptor::OnReadPacket(rtc::PacketTransportInternal* transport,
                                       const char* data,
                                       size_t size,
                                       const int64_t& packet_time_us,
                                       int flags) {
  // Only used in bypass mode.
  RTC_DCHECK(FLAG_bypass_datagram_dtls_test_only);

  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(transport == ice_transport_.get());
  RTC_DCHECK(flags == 0);

  PropagateReadPacket(rtc::ArrayView<const uint8_t>(
                          reinterpret_cast<const uint8_t*>(data), size),
                      packet_time_us);
}

void DatagramDtlsAdaptor::OnDatagramReceived(
    rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!FLAG_bypass_datagram_dtls_test_only);

  // TODO(sukhanov): I am not filling out time, but on my video quality
  // test in WebRTC the time was not set either and higher layers of the stack
  // overwrite -1 with current current rtc time. Leaveing comment for now to
  // make sure it works as expected.
  int64_t packet_time_us = -1;

  PropagateReadPacket(data, packet_time_us);
}

void DatagramDtlsAdaptor::OnDatagramSent(webrtc::DatagramId datagram_id) {
  // When we called DatagramTransportInterface::SendDatagram, we passed
  // packet_id as datagram_id, so we simply need to set it in sent_packet
  // and propagate notification up the stack.

  // Also see how DatagramDtlsAdaptor::OnSentPacket handles OnSentPacket
  // notification from ICE in bypass mode.
  rtc::SentPacket sent_packet(/*packet_id=*/datagram_id, rtc::TimeMillis());

  PropagateOnSentNotification(sent_packet);
}

void DatagramDtlsAdaptor::OnSentPacket(rtc::PacketTransportInternal* transport,
                                       const rtc::SentPacket& sent_packet) {
  // Only used in bypass mode.
  RTC_DCHECK(FLAG_bypass_datagram_dtls_test_only);
  RTC_DCHECK_RUN_ON(&thread_checker_);

  PropagateOnSentNotification(sent_packet);
}

void DatagramDtlsAdaptor::PropagateOnSentNotification(
    const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalSentPacket(this, sent_packet);
}

void DatagramDtlsAdaptor::PropagateReadPacket(
    rtc::ArrayView<const uint8_t> data,
    const int64_t& packet_time_us) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalReadPacket(this, reinterpret_cast<const char*>(data.data()),
                   data.size(), packet_time_us, /*flags=*/0);
}

int DatagramDtlsAdaptor::component() const {
  return kDatagramDtlsAdaptorComponent;
}
bool DatagramDtlsAdaptor::IsDtlsActive() const {
  // TODO(sukhanov): Returning true, because we have "crypto", although it is
  // provided by datagram transport. Make sure it's the right assumption.
  // The only place that calls IsDtlsActive besides tests is media_session:
  // https://cs.chromium.org/search/?q=IsDtlsActive
  return true;
}
bool DatagramDtlsAdaptor::GetDtlsRole(rtc::SSLRole* role) const {
  return false;
}
bool DatagramDtlsAdaptor::SetDtlsRole(rtc::SSLRole role) {
  return false;
}
bool DatagramDtlsAdaptor::GetSrtpCryptoSuite(int* cipher) {
  return false;
}
bool DatagramDtlsAdaptor::GetSslCipherSuite(int* cipher) {
  return false;
}

rtc::scoped_refptr<rtc::RTCCertificate>
DatagramDtlsAdaptor::GetLocalCertificate() const {
  return rtc::scoped_refptr<rtc::RTCCertificate>();
}

bool DatagramDtlsAdaptor::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return false;
}

std::unique_ptr<rtc::SSLCertChain> DatagramDtlsAdaptor::GetRemoteSSLCertChain()
    const {
  return std::unique_ptr<rtc::SSLCertChain>();
}

bool DatagramDtlsAdaptor::ExportKeyingMaterial(const std::string& label,
                                               const uint8_t* context,
                                               size_t context_len,
                                               bool use_context,
                                               uint8_t* result,
                                               size_t result_len) {
  return false;
}

bool DatagramDtlsAdaptor::SetRemoteFingerprint(const std::string& digest_alg,
                                               const uint8_t* digest,
                                               size_t digest_len) {
  // TODO(sukhanov): We probably should not called with fingerptints in
  // datagram scenario, but we may need to change code up the stack before
  // we can return false or DCHECK.
  return true;
}

bool DatagramDtlsAdaptor::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  // TODO(sukhanov): We may be able to return false and/or DCHECK that we
  // are not called if datagram transport is used, but we need to change
  // integration before we can do it.
  return true;
}

IceTransportInternal* DatagramDtlsAdaptor::ice_transport() {
  return ice_transport_.get();
}

void DatagramDtlsAdaptor::OnReadyToSend(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (is_writable_) {
    SignalWritableState(this);
    SignalReadyToSend(this);
  }
}

void DatagramDtlsAdaptor::OnWritableState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  is_writable_ = ice_transport_->writable();
  if (is_writable_) {
    SignalWritableState(this);
    SignalReadyToSend(this);
  }
}

DtlsTransportState DatagramDtlsAdaptor::dtls_state() const {
  return dtls_state_;
}

void DatagramDtlsAdaptor::OnStateChanged(webrtc::MediaTransportState state) {
  DtlsTransportState dtls_state_new = DTLS_TRANSPORT_NEW;
  switch (state) {
    case webrtc::MediaTransportState::kWritable:
      dtls_state_new = DTLS_TRANSPORT_CONNECTED;
      is_writable_ = true;
      break;

    case webrtc::MediaTransportState::kPending:
      dtls_state_new = DTLS_TRANSPORT_CONNECTING;
      break;

    case webrtc::MediaTransportState::kClosed:
      dtls_state_new = DTLS_TRANSPORT_CLOSED;
      break;
  }

  // Signal up if state is changed.
  if (dtls_state_new != dtls_state_) {
    dtls_state_ = dtls_state_new;
    SignalDtlsState(this, dtls_state_);
  }

  if (is_writable_) {
    SignalWritableState(this);
    SignalReadyToSend(this);
    SignalReceivingState(this);
  }
}

const std::string& DatagramDtlsAdaptor::transport_name() const {
  return ice_transport_->transport_name();
}

bool DatagramDtlsAdaptor::writable() const {
  return ice_transport_->writable();
}

bool DatagramDtlsAdaptor::receiving() const {
  return ice_transport_->receiving();
}

int DatagramDtlsAdaptor::SetOption(rtc::Socket::Option opt, int value) {
  return ice_transport_->SetOption(opt, value);
}

int DatagramDtlsAdaptor::GetError() {
  return ice_transport_->GetError();
}

void DatagramDtlsAdaptor::OnNetworkRouteChanged(
    absl::optional<rtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalNetworkRouteChanged(network_route);
}

void DatagramDtlsAdaptor::OnReceivingState(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  SignalReceivingState(this);
}

}  // namespace cricket
