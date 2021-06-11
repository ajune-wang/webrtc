/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RECEIVE_STREAM_H_
#define CALL_RECEIVE_STREAM_H_

#include <vector>

#include "api/crypto/frame_decryptor_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/transport/rtp/rtp_source.h"

namespace webrtc {

// Common base interface for AudioReceiveStream and VideoReceiveStream.
class ReceiveStream {
 public:
  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;

  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  virtual void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<webrtc::FrameTransformerInterface>
          frame_transformer) = 0;

  virtual void SetFrameDecryptor(
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) = 0;

  virtual std::vector<RtpSource> GetSources() const = 0;

 protected:
  virtual ~ReceiveStream() {}
};
}  // namespace webrtc

#endif  // CALL_RECEIVE_STREAM_H_
