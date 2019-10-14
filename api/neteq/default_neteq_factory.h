/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_DEFAULT_NETEQ_FACTORY_H_
#define API_NETEQ_DEFAULT_NETEQ_FACTORY_H_

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/neteq/neteq_controller_factory.h"
#include "api/neteq/neteq_factory.h"

namespace webrtc {

// This NetEq factory will use WebRTC's built-in AudioDecoders as well as the
// built-in NetEqController logic.
class DefaultNetEqFactory : public NetEqFactory {
 public:
  DefaultNetEqFactory();
  ~DefaultNetEqFactory() override;
  DefaultNetEqFactory(const DefaultNetEqFactory&) = delete;
  DefaultNetEqFactory& operator=(const DefaultNetEqFactory&) = delete;

  std::unique_ptr<NetEq> CreateNetEq(const NetEq::Config& config,
                                     Clock* clock) const override;

 private:
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  std::unique_ptr<NetEqControllerFactory> controller_factory_;
};

}  // namespace webrtc
#endif  // API_NETEQ_DEFAULT_NETEQ_FACTORY_H_
