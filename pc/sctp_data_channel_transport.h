/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_DATA_CHANNEL_TRANSPORT_H_
#define PC_SCTP_DATA_CHANNEL_TRANSPORT_H_

#include "api/rtc_error.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "media/sctp/sctp_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

// SCTP implementation of DataChannelTransportInterface.
class SctpDataChannelTransport : public DataChannelTransportInterface,
                                 public DataChannelSink {
 public:
  explicit SctpDataChannelTransport(
      const rtc::WeakPtr<cricket::SctpTransportInternal>& sctp_transport);

  RTCError OpenChannel(int channel_id) override;
  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override;
  RTCError CloseChannel(int channel_id) override;
  void SetDataSink(DataChannelSink* sink) override;
  bool IsReadyToSend() const override;

 private:
  // DataChannelSink implementation - proxying calls to DataChannelController.
  void OnDataReceived(int channel_id,
                      DataMessageType type,
                      const rtc::CopyOnWriteBuffer& buffer) override;
  void OnChannelClosing(int channel_id) override;
  void OnChannelClosed(int channel_id) override;
  void OnReadyToSend() override;
  void OnTransportClosed(RTCError error) override;

  rtc::WeakPtr<cricket::SctpTransportInternal> sctp_transport_;

  DataChannelSink* sink_ = nullptr;
  bool ready_to_send_ = false;
};

}  // namespace webrtc

#endif  // PC_SCTP_DATA_CHANNEL_TRANSPORT_H_
