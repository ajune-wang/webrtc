/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_PACKET_RECEIVER_H_
#define CALL_PACKET_RECEIVER_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/media_types.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

class PacketReceiver {
 public:
  enum DeliveryStatus {
    DELIVERY_OK,
    DELIVERY_UNKNOWN_SSRC,
    DELIVERY_PACKET_ERROR,
  };

  // Definition of the callback to issue when packet delivery could not be
  // completed on the grounds of the SSRC being uknown (status should be assumed
  // to be DELIVERY_UNKNOWN_SSRC).
  // An implementation of this callback can choose to retry delivery.
  typedef std::function<void(MediaType, rtc::CopyOnWriteBuffer, int64_t)>
      UnknownSsrcCallback;

  // Asynchronously handle packet delivery and report back to the caller when
  // delivery of the packet has completed.
  // Note that if the packet is invalid or can be processed without the need of
  // asynchronous operations that the |callback| may have been called before
  // the function returns.
  // TODO(bugs.webrtc.org/11993): This function is meant to be called on the
  // network thread exclusively but while the code is being updated to align
  // with those goals, it may be called either on the worker or network threads.
  // Update docs etc when the work has been completed. Once we're done with the
  // updates, we might be able to go back to returning the status from this
  // function instead of having to report it via a callback.
  // TODO(bugs.webrtc.org/11993): The callback isn't actually needed except for
  // when the SSRC is unknown (DELIVERY_UNKNOWN_SSRC). We shouldn't require
  // a callback to be made for a successfully delivered packet or if a packet
  // error was encountered, since in such a case, we won't retry the packet
  // or otherwise need to asynchronously handle the issue.
  virtual void DeliverPacketAsync(MediaType media_type,
                                  rtc::CopyOnWriteBuffer packet,
                                  int64_t packet_time_us,
                                  UnknownSsrcCallback callback) {
    // TODO(bugs.webrtc.org/11993): Remove this default implementation.
    DeliveryStatus status = DeliverPacket(media_type, packet, packet_time_us);
    if (status == DELIVERY_UNKNOWN_SSRC && callback)
      callback(media_type, std::move(packet), packet_time_us);
  }

  virtual DeliveryStatus DeliverPacket(MediaType media_type,
                                       rtc::CopyOnWriteBuffer packet,
                                       int64_t packet_time_us) = 0;

 protected:
  virtual ~PacketReceiver() {}
};

}  // namespace webrtc

#endif  // CALL_PACKET_RECEIVER_H_
