/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_data_channel_transport.h"

#include "rtc_base/checks.h"

namespace webrtc {

SctpDataChannelTransport::SctpDataChannelTransport(
    const rtc::WeakPtr<cricket::SctpTransportInternal>& sctp_transport)
    : sctp_transport_(sctp_transport) {
  RTC_DCHECK(sctp_transport_);
}

RTCError SctpDataChannelTransport::OpenChannel(int channel_id) {
  RTC_DCHECK(sctp_transport_);
  sctp_transport_->OpenStream(channel_id);
  return RTCError::OK();
}

RTCError SctpDataChannelTransport::SendData(
    int channel_id,
    const SendDataParams& params,
    const rtc::CopyOnWriteBuffer& buffer) {
  cricket::SendDataResult result;
  RTC_DCHECK(sctp_transport_);
  sctp_transport_->SendData(channel_id, params, buffer, &result);

  // TODO(mellem):  See about changing the interfaces to not require mapping
  // SendDataResult to RTCError and back again.
  switch (result) {
    case cricket::SendDataResult::SDR_SUCCESS:
      return RTCError::OK();
    case cricket::SendDataResult::SDR_BLOCK: {
      // Send buffer is full.
      ready_to_send_ = false;
      return RTCError(RTCErrorType::RESOURCE_EXHAUSTED);
    }
    case cricket::SendDataResult::SDR_ERROR:
      return RTCError(RTCErrorType::NETWORK_ERROR);
  }
  return RTCError(RTCErrorType::NETWORK_ERROR);
}

RTCError SctpDataChannelTransport::CloseChannel(int channel_id) {
  RTC_DCHECK(sctp_transport_);
  sctp_transport_->ResetStream(channel_id);
  return RTCError::OK();
}

void SctpDataChannelTransport::SetDataSink(DataChannelSink* sink) {
  sink_ = sink;
  RTC_DCHECK(sctp_transport_);
  sctp_transport_->SetDataChannelSink(sink_ ? this : nullptr);
  if (sink_ && ready_to_send_) {
    sink_->OnReadyToSend();
  }
}

bool SctpDataChannelTransport::IsReadyToSend() const {
  return ready_to_send_;
}

void SctpDataChannelTransport::OnDataReceived(
    int channel_id,
    DataMessageType type,
    const rtc::CopyOnWriteBuffer& buffer) {
  if (sink_) {
    sink_->OnDataReceived(channel_id, type, buffer);
  }
}

void SctpDataChannelTransport::OnChannelClosing(int channel_id) {
  if (sink_) {
    sink_->OnChannelClosing(channel_id);
  }
}

void SctpDataChannelTransport::OnChannelClosed(int channel_id) {
  if (sink_) {
    sink_->OnChannelClosed(channel_id);
  }
}

void SctpDataChannelTransport::OnReadyToSend() {
  ready_to_send_ = true;
  if (sink_) {
    sink_->OnReadyToSend();
  }
}

void SctpDataChannelTransport::OnTransportClosed(RTCError error) {
  if (sink_) {
    sink_->OnTransportClosed(error);
  }
}

}  // namespace webrtc
