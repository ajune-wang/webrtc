/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_TEMPLATE_H_
#define API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_TEMPLATE_H_

#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory_impl.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"

namespace webrtc {
// Make an AudioEncoderFactory that can create instances of the given encoders.
//
// Each encoder type is given as a template argument to the function; it should
// be a struct with the following static member functions:
//
//   // Converts `audio_format` to a ConfigType instance. Returns an empty
//   // optional if `audio_format` doesn't correctly specify an encoder of our
//   // type.
//   absl::optional<ConfigType> SdpToConfig(const SdpAudioFormat& audio_format);
//
//   // Appends zero or more AudioCodecSpecs to the list that will be returned
//   // by AudioEncoderFactory::GetSupportedEncoders().
//   void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs);
//
//   // Returns information about how this format would be encoded. Used to
//   // implement AudioEncoderFactory::QueryAudioEncoder().
//   AudioCodecInfo QueryAudioEncoder(const ConfigType& config);
//
//   // Creates an AudioEncoder for the specified format. Used to implement
//   // AudioEncoderFactory::MakeAudioEncoder().
//   std::unique_ptr<AudioEncoder> MakeAudioEncoder(
//       const ConfigType& config,
//       int payload_type,
//       absl::optional<AudioCodecPairId> codec_pair_id);
//
// ConfigType should be a type that encapsulates all the settings needed to
// create an AudioEncoder. T::Config (where T is the encoder struct) should
// either be the config type, or an alias for it.
//
// Whenever it tries to do something, the new factory will try each of the
// encoders in the order they were specified in the template argument list,
// stopping at the first one that claims to be able to do the job.
//
// TODO(kwiberg): Point at CreateBuiltinAudioEncoderFactory() for an example of
// how it is used.
template <typename... Ts>
rtc::scoped_refptr<AudioEncoderFactory> CreateAudioEncoderFactory(
    const FieldTrialsView* field_trials = nullptr) {
  // There's no technical reason we couldn't allow zero template parameters,
  // but such a factory couldn't create any encoders, and callers can do this
  // by mistake by simply forgetting the <> altogether. So we forbid it in
  // order to prevent caller foot-shooting.
  static_assert(sizeof...(Ts) >= 1,
                "Caller must give at least one template parameter");

  return rtc::make_ref_counted<
      audio_encoder_factory_template_impl::AudioEncoderFactoryT<Ts...>>(
      field_trials);
}

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_TEMPLATE_H_
