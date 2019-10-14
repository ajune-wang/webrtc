/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/neteq/default_neteq_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/neteq/default_neteq_controller_factory.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/neteq_impl.h"

namespace webrtc {

DefaultNetEqFactory::DefaultNetEqFactory()
    : decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      controller_factory_(std::make_unique<DefaultNetEqControllerFactory>()) {}
DefaultNetEqFactory::~DefaultNetEqFactory() = default;

std::unique_ptr<NetEq> DefaultNetEqFactory::CreateNetEq(
    const NetEq::Config& config,
    Clock* clock) const {
  return std::make_unique<NetEqImpl>(
      config, NetEqImpl::Dependencies(config, clock, decoder_factory_,
                                      controller_factory_));
}

}  // namespace webrtc
