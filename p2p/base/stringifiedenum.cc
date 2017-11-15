/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stringifiedenum.h"

#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"

namespace webrtc {

namespace icelog {

std::vector<std::string> TokenizeArgString(const std::string& args_str) {
  std::vector<std::string> arg_tokens;
  size_t num_args = rtc::tokenize(std::string(args_str), ',', &arg_tokens);
  for (size_t i = 0; i < num_args; i++) {
    arg_tokens[i] = rtc::string_trim(arg_tokens[i]);
  }
  return arg_tokens;
}

}  // namespace icelog

}  // namespace webrtc
