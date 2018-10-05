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

#include "absl/strings/string_view.h"

namespace webrtc {

// Encode/decode a sequence of strings, whose length is not known to be
// discernable from the blob itself (i.e. without being transmitted OOB),
// in a away that would allow us to separate them again on the decoding side.
//
// EncodeBlobs() must be given a non-empty vector. The blobs themselves may
// be equal to "", though.
// EncodeBlobs() may not fail.
// EncodeBlobs() never returns the empty string.
//
// DecodeBlobs() must be called on a non-empty string, and |num_of_blobs| must
// be greater than zero.
// DecodeBlobs() returns an empty vector if it fails, e.g. due to a mismatch
// between |num_of_blobs| and |encoded_blobs|, which can happen if
// |encoded_blobs| is corrupted.
// When successful, DecodeBlobs() returns a vector of string_view objects,
// which refer to the original input (|encoded_blobs|), and therefore may
// not outlive it.
std::string EncodeBlobs(const std::vector<absl::string_view>& blobs);
std::vector<absl::string_view> DecodeBlobs(absl::string_view encoded_blobs,
                                           size_t num_of_blobs);

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_BLOB_ENCODING_H_
