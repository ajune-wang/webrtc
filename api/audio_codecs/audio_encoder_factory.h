/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_
#define API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// A factory that creates AudioEncoders.
class AudioEncoderFactory : public rtc::RefCountInterface {
 public:
  // Returns a prioritized list of audio codecs, to use for signaling etc.
  virtual std::vector<AudioCodecSpec> GetSupportedEncoders() = 0;

  // Returns information about how this format would be encoded, provided it's
  // supported. More format and format variations may be supported than those
  // returned by GetSupportedEncoders().
  virtual absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) = 0;

  // Creates AudioEncoder for the specificed format by using virtual function
  // CreateAudioEncoder
  // Having separate functions to call and to implement simplify evolving the
  // interface as callers and implementations can be updated independently.
  struct OptionalCreateParameters {
    // Payload type for the encoder to tag its payloads with.
    // TODO(ossu): Try to avoid audio encoders having to know their payload
    // type.
    absl::optional<int> payload_type;

    // Links encoders and decoders that talk to the same remote entity: if
    // a AudioEncoderFactory::Create() and a
    // AudioDecoderFactory::MakeAudioDecoder() call receive non-null IDs that
    // compare equal, the factory implementations may assume that the encoder
    // and decoder form a pair. (The intended use case for this is to set up
    // communication between the AudioEncoder and AudioDecoder instances, which
    // is needed for some codecs with built-in bandwidth adaptation.)
    // Note: Implementations need to be robust against combinations other than
    // one encoder, one decoder getting the same ID; such encoders must still
    // work.
    absl::optional<AudioCodecPairId> codec_pair_id;
  };
  absl::Nullable<std::unique_ptr<AudioEncoder>> Create(
      const Environment& env,
      const SdpAudioFormat& format,
      OptionalCreateParameters options = {});

  class CreateParameters {
   public:
    // This class has no public constructors. The intended users of this
    // class are implementations of the CreateAudioEncoder. They may query
    // construction parameters, or pass them by reference to delegate
    // construction of an AudioEncoder to another factory.
    CreateParameters() = delete;

    CreateParameters(const CreateParameters&) = delete;
    CreateParameters& operator=(const CreateParameters&) = delete;

    const Environment& env() const { return env_; }
    const SdpAudioFormat& format() const { return format_; }
    int payload_type() const;
    absl::optional<AudioCodecPairId> codec_pair_id() const;

   private:
    friend class AudioEncoderFactory;
    CreateParameters(const Environment& env,
                     const SdpAudioFormat& format,
                     const OptionalCreateParameters& o);

    const Environment& env_;
    const SdpAudioFormat& format_;
    OptionalCreateParameters options_;
  };
  // TODO: bugs.webrtc.org/343086059 - Make pure virtual when all
  // implementations are updated.
  virtual absl::Nullable<std::unique_ptr<AudioEncoder>> CreateAudioEncoder(
      const CreateParameters& p);

  // TODO: bugs.webrtc.org/343086059 - Update all callers to use `Create`
  // instead, update implementations to override `CreateAudioEncoder` instead,
  // then delete the `MakeAudioEncoder`.
  virtual std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id);
};

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

inline int AudioEncoderFactory::CreateParameters::payload_type() const {
  if (options_.payload_type.has_value()) {
    return *options_.payload_type;
  }
  // LOG(WARNING) << Using payload type, but it wasn't provided.
  return -1;
}

inline absl::optional<AudioCodecPairId>
AudioEncoderFactory::CreateParameters::codec_pair_id() const {
  return options_.codec_pair_id;
}

inline AudioEncoderFactory::CreateParameters::CreateParameters(
    const Environment& env,
    const SdpAudioFormat& format,
    const OptionalCreateParameters& o)
    : env_(env), format_(format), options_(o) {
  if (options_.payload_type.has_value() &&
      (*options_.payload_type < 0 || *options_.payload_type > 127)) {
    //    LOG(ERROR) << Invalide payload type provided.
    options_.payload_type = absl::nullopt;
  }
}

inline absl::Nullable<std::unique_ptr<AudioEncoder>>
AudioEncoderFactory::CreateAudioEncoder(const CreateParameters& p) {
  return MakeAudioEncoder(p.payload_type(), p.format(), p.codec_pair_id());
}

inline absl::Nullable<std::unique_ptr<AudioEncoder>>
AudioEncoderFactory::Create(const Environment& env,
                            const SdpAudioFormat& format,
                            OptionalCreateParameters options) {
  return CreateAudioEncoder(CreateParameters(env, format, options));
}

inline std::unique_ptr<AudioEncoder> AudioEncoderFactory::MakeAudioEncoder(
    int payload_type,
    const SdpAudioFormat& format,
    absl::optional<AudioCodecPairId> codec_pair_id) {
  // Newer code shouldn't call it.
  // Older code should implement it.
  RTC_CHECK_NOTREACHED();
}

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_
