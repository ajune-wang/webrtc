/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_RIDDESCRIPTION_H_
#define MEDIA_BASE_RIDDESCRIPTION_H_

#include <map>
#include <string>
#include <vector>

namespace cricket {

enum RidDirection { kRidDirectionSend, kRidDirectionReceive };

// Description of a Restriction Id (RID) according to:
// https://tools.ietf.org/html/draft-ietf-mmusic-rid-15
// A Restriction Identifier serves two purposes:
//   1. Uniquely identifies an RTP stream inside an RTP session.
//      When combined with MIDs (https://tools.ietf.org/html/rfc5888),
//      RIDs uniquely identify an RTP stream within an RTP session.
//      The MID will identify the media section and the RID will identify
//      the stream within the section.
//      RID identifiers must be unique within the media section.
//   2. Allows indicating further restrictions to the stream.
//      The following are some examples of these restrictions:
//        a. max-width, max-height, max-fps, max-br, ...
//        b. further restricting the codec set (from what m= section specified)
//
// Note: Indicating dependencies between streams (using depend) will not be
// supported, since the WG is adopting a different approach to achieve this.
// As of 2018-12-04, the new SVC (Scalable Video Coder) approach is still not
// mature enough to be implemented as part of this work.
// See: https://w3c.github.io/webrtc-svc/ for more details.
struct RidDescription final {
  RidDescription();
  RidDescription(const std::string& rid, RidDirection direction);
  RidDescription(const RidDescription& other);
  ~RidDescription();
  RidDescription& operator=(const RidDescription& other);

  // This is currently required for unit tests of StreamParams which contains
  // RidDescription objects and checks for equality using operator==.
  bool operator==(const RidDescription& other) const;
  bool operator!=(const RidDescription& other) const {
    return !(*this == other);
  }

  // The RID identifier that uniquely identifies the stream within the session.
  std::string rid;

  // Specifies the direction for which the specified restrictions hold.
  // This direction is either send or receive and is independent of the
  // direction of the transceiver.
  RidDirection direction;

  // The list of supported formats for this stream.
  // It should be a subset of the formats supported for the media section.
  std::vector<int> formats;

  // Contains key-value pairs for restrictions.
  // The keys are not validated against a known set.
  // The meaning to infer for the values depends on each key.
  // Examples:
  // 1. An entry for max-width will have a value that is interpreted as an int.
  // 2. An entry for max-bpp (bits per pixel) will have a float value.
  // Interpretation (and validation of value) is left for the implementation.
  // I.E. the media engines should validate values for parameters they support.
  std::map<std::string, std::string> restrictions;
};

}  // namespace cricket

#endif  // MEDIA_BASE_RIDDESCRIPTION_H_
