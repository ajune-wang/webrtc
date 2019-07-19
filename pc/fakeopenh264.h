/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef FAKEOPENH264_H_
#define FAKEOPENH264_H_

#include <stdio.h>
#include <type_traits>
#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_ver.h"

class ISVCEncoder;
typedef int (*CreateH264Encoder)(ISVCEncoder** ppEncoder);
typedef void (*DestroyH264Encoder)(ISVCEncoder* pEncoder);
typedef long (*CreateH264Decoder)(ISVCDecoder** ppDecoder);
typedef void (*DestroyH264Decoder)(ISVCDecoder* pDecoder);

extern "C" {
int WelsCreateSVCEncoder(ISVCEncoder** ppEncoder);
void WelsDestroySVCEncoder(ISVCEncoder* pEncoder);
long WelsCreateDecoder(ISVCDecoder** ppDecoder);
void WelsDestroyDecoder(ISVCDecoder* pDecoder);
}

void loadLib();
void closeLib();
bool amIloaded();
void setLibPath();
#endif  // FAKEOPENH264_H_