/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_CODEC_COMPARISON_H_
#define MEDIA_BASE_CODEC_COMPARISON_H_

#include <string>

namespace cricket {

bool IsSameCodecSpecific(const std::string& name1,
                         const webrtc::CodecParameterMap& params1,
                         const std::string& name2,
                         const webrtc::CodecParameterMap& params2);

}  // namespace cricket

#endif  // MEDIA_BASE_CODEC_COMPARISON_H_
