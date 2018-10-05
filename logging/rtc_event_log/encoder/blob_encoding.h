/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_BLOB_ENCODING_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_BLOB_ENCODING_H_

#include <string>
#include <vector>

namespace webrtc {

// Encode/decode a sequence of strings, whose length is not known to be
// discernable from the blob itself (i.e. without being transmitted OOB),
// in a away that would allow us to separate them again on the decoding side.
std::string EncodeBlobs(const std::vector<std::string>& blobs);
std::vector<std::string> DecodeBlobs(const std::string& encoded_blobs,
                                     size_t num_of_blobs);

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_BLOB_ENCODING_H_
