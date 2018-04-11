/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_APM_MESSAGE_H_
#define MODULES_AUDIO_PROCESSING_APM_MESSAGE_H_

namespace webrtc {

// APM message.
struct ApmMessage {
  // NEXT_AVAILABLE_APM_MESSAGE_ID: 2.
  // If you add a new APM message ID, make sure that you also increment
  // NEXT_AVAILABLE_APM_MESSAGE_ID. Do not delete entries, but deprecate them.
  // Replace an entry by deprecating and adding a new one.
  // An entry name must end with e.g., _INT_VAL if the payload is read by
  // accessing to int_val. This is done to possibly reduce wrong type
  // errors while accessing to the right field of the union used for the
  // payload.
  enum {
    TEST = 0,  // Only used for testing with any payload type.
    UPDATE_CAPTURE_PRE_GAIN_FLOAT_VAL = 1,
  } id;
  // Payload.
  union {
    int int_val;
    float float_val;
  };
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_APM_MESSAGE_H_
