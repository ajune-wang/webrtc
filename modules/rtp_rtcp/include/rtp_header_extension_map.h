/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtp_parameters.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Keeps id<->uri mapping for the rtp header extensions
class RtpHeaderExtensionMap {
 public:
  static constexpr absl::string_view kInvalidUri = "";
  static constexpr int kInvalidId = 0;

  RtpHeaderExtensionMap();
  explicit RtpHeaderExtensionMap(bool extmap_allow_mixed);
  explicit RtpHeaderExtensionMap(rtc::ArrayView<const RtpExtension> extensions);

  static rtc::ArrayView<const absl::string_view> KnownExtensions();

  void Reset(rtc::ArrayView<const RtpExtension> extensions);

  template <typename Extension>
  bool Register(int id) {
    return UnsafeRegisterByUri(id, Extension::Uri());
  }
  // Registers an extension known by webrtc library.
  bool RegisterByUri(int id, absl::string_view uri);

  template <typename Extension>
  bool IsRegistered() const {
    return Id<Extension>() != kInvalidId;
  }
  bool IsRegistered(absl::string_view uri) const {
    return Id(uri) != kInvalidId;
  }

  // Returns uri of the registered extension, or an empty string view if
  // id is not used.
  absl::string_view Uri(int id) const;

  // Returns id of the the registered extension, or `kInvalidId` if extension is
  // not registered.
  template <typename Extension>
  int Id() const {
    return UnsafeId(Extension::Uri());
  }

  int Id(absl::string_view uri) const;

  void Deregister(absl::string_view uri);

  // Corresponds to the SDP attribute extmap-allow-mixed, see RFC8285.
  // Set to true if it's allowed to mix one- and two-byte RTP header extensions
  // in the same stream.
  bool ExtmapAllowMixed() const { return extmap_allow_mixed_; }
  void SetExtmapAllowMixed(bool extmap_allow_mixed) {
    extmap_allow_mixed_ = extmap_allow_mixed;
  }

  template <typename Functor>
  void ListRegisteredExtensions(const Functor& f) const {
    for (const auto& entry : mapping_) {
      f(entry.id, entry.uri);
    }
  }

 protected:
  // `uri` must point to a string that is valid until end of process/while any
  // of webrtc functions are are used, in particular uri might be used by webrtc
  // code after `this` is destroyed. Two uris that points to the same string
  // should also have to point to the same memory. Can be used to register rtp
  // header extension unkown to the webrtc library.
  bool UnsafeRegisterByUri(int id, absl::string_view uri);

  // Returns id of the the registered extension, or 0 if uri is not registered.
  // uri must points to the same memory as string passed to UnsafeRegisterByUri
  int UnsafeId(absl::string_view uri) const;

 private:
  struct Entry {
    int id;
    absl::string_view uri;
  };
  std::vector<Entry> mapping_;
  bool extmap_allow_mixed_ = false;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_EXTENSION_MAP_H_
