/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediatypes.h"

#include "rtc_base/checks.h"

namespace cricket {

const char kMediaTypeVideo[] = "video";
const char kMediaTypeAudio[] = "audio";
const char kMediaTypeData[] = "data";

std::string MediaTypeToString(MediaType type) {
  switch (type) {
    case MEDIA_TYPE_AUDIO:
      return kMediaTypeAudio;
    case MEDIA_TYPE_VIDEO:
      return kMediaTypeVideo;
    case MEDIA_TYPE_DATA:
      return kMediaTypeData;
  }
  FATAL();
  // Not reachable; avoids compile warning.
  return "";
}

MediaType MediaTypeFromString(const std::string& type_str) {
  if (type_str == kMediaTypeAudio) {
    return MEDIA_TYPE_AUDIO;
  } else if (type_str == kMediaTypeVideo) {
    return MEDIA_TYPE_VIDEO;
  } else if (type_str == kMediaTypeData) {
    return MEDIA_TYPE_DATA;
  }
  FATAL();
  // Not reachable; avoids compile warning.
  return static_cast<MediaType>(-1);
}

}  // namespace cricket
