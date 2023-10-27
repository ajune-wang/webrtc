/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_H_
#define API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_H_

#include <memory>

#include "api/media_engine/media_engine_factory_interface.h"

namespace webrtc {

std::unique_ptr<MediaEngineFactoryInterface> CreateMediaEngineFactory();

}  // namespace webrtc

#endif  // API_MEDIA_ENGINE_MEDIA_ENGINE_FACTORY_H_
