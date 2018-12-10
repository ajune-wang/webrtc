/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdpserializer.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "api/jsep.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/strings/string_builder.h"

using cricket::RidDescription;
using cricket::RidDirection;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;

namespace webrtc {

namespace {

// delimiters
const char kDelimiterComma[] = ",";
const char kDelimiterCommaChar = ',';
const char kDelimiterEqual[] = "=";
const char kDelimiterEqualChar = '=';
const char kDelimiterSemicolon[] = ";";
const char kDelimiterSemicolonChar = ';';
const char kDelimiterSpace[] = " ";
const char kDelimiterSpaceChar = ' ';

// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
// https://tools.ietf.org/html/draft-ietf-mmusic-rid-15#section-10
const char kSimulcastPausedStream[] = "~";
const char kSimulcastPausedStreamChar = '~';
const char kSendDirection[] = "send";
const char kReceiveDirection[] = "recv";
const char kPayloadType[] = "pt";

RTCError ParseError(const std::string& message) {
  return RTCError(RTCErrorType::SYNTAX_ERROR, message);
}

// These methods serialize simulcast according to the specification:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
rtc::StringBuilder& operator<<(rtc::StringBuilder& builder,
                               const SimulcastLayer& simulcast_layer) {
  if (simulcast_layer.is_paused) {
    builder << kSimulcastPausedStream;
  }
  builder << simulcast_layer.rid;
  return builder;
}

rtc::StringBuilder& operator<<(
    rtc::StringBuilder& builder,
    const std::vector<SimulcastLayer>& layer_alternatives) {
  bool first = true;
  for (const SimulcastLayer& rid : layer_alternatives) {
    if (!first) {
      builder << kDelimiterComma;
    }
    builder << rid;
    first = false;
  }
  return builder;
}

rtc::StringBuilder& operator<<(rtc::StringBuilder& builder,
                               const SimulcastLayerList& simulcast_layers) {
  bool first = true;
  for (auto alternatives : simulcast_layers) {
    if (!first) {
      builder << kDelimiterSemicolon;
    }
    builder << alternatives;
    first = false;
  }
  return builder;
}
// This method deserializes simulcast according to the specification:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
// sc-str-list  = sc-alt-list *( ";" sc-alt-list )
// sc-alt-list  = sc-id *( "," sc-id )
// sc-id-paused = "~"
// sc-id        = [sc-id-paused] rid-id
// rid-id       = 1*(alpha-numeric / "-" / "_") ; see: I-D.ietf-mmusic-rid
RTCErrorOr<SimulcastLayerList> ParseSimulcastLayerList(const std::string& str) {
  std::vector<std::string> tokens;
  rtc::tokenize_with_empty_tokens(str, kDelimiterSemicolonChar, &tokens);
  if (tokens.empty()) {
    return ParseError("Layer list cannot be empty.");
  }

  SimulcastLayerList result;
  for (const std::string& token : tokens) {
    if (token.empty()) {
      return ParseError("Simulcast alternative layer list is empty.");
    }

    std::vector<std::string> rid_tokens;
    rtc::tokenize_with_empty_tokens(token, kDelimiterCommaChar, &rid_tokens);
    if (rid_tokens.empty()) {
      return ParseError("Simulcast alternative layer list is malformed.");
    }

    std::vector<SimulcastLayer> layers;
    for (const auto& rid_token : rid_tokens) {
      if (rid_token.empty() || rid_token == kSimulcastPausedStream) {
        return ParseError("Rid must not be empty.");
      }

      bool paused = rid_token[0] == kSimulcastPausedStreamChar;
      std::string rid = paused ? rid_token.substr(1) : rid_token;
      layers.push_back(SimulcastLayer(rid, paused));
    }

    result.AddLayerWithAlternatives(layers);
  }

  return std::move(result);
}

webrtc::RTCError ParseRidFormatList(const std::string& format_list,
                                    RidDescription* rid_description) {
  RTC_DCHECK(rid_description);
  std::vector<int>& formats = rid_description->formats;
  // Validate that the description doesn't already have formats or restrictions.
  // If the pt= field is specified, it must be first and must not repeat.
  if (!formats.empty()) {
    return ParseError("Multiple pt= found in Rid Description.");
  }
  if (!rid_description->restrictions.empty()) {
    return ParseError("Format list must appear first in the restrictions.");
  }

  // If the pt= field is specified, it must have a value.
  if (format_list.empty()) {
    return ParseError("Format list must have at least one value.");
  }

  // Tokenize the ',' delimited list
  std::vector<std::string> string_formats;
  rtc::tokenize(format_list, kDelimiterCommaChar, &string_formats);
  if (string_formats.empty()) {
    return ParseError("Format list must have at least one value.");
  }

  for (const std::string& format : string_formats) {
    absl::optional<int> value = rtc::StringToNumber<int>(format);
    if (!value.has_value()) {
      return ParseError("Invalid format specifier: " + format);
    }

    // check if the value already appears in the format list
    if (std::find(formats.begin(), formats.end(), value.value()) !=
        formats.end()) {
      return ParseError("Duplicate format in list: " + format);
    }
    formats.push_back(value.value());
  }

  return RTCError();
}

}  // namespace

std::string SdpSerializer::SerializeSimulcastDescription(
    const cricket::SimulcastDescription& simulcast) const {
  rtc::StringBuilder sb;
  std::string delimiter;

  if (!simulcast.send_layers().empty()) {
    sb << kSendDirection << kDelimiterSpace << simulcast.send_layers();
    delimiter = kDelimiterSpace;
  }

  if (!simulcast.receive_layers().empty()) {
    sb << delimiter << kReceiveDirection << kDelimiterSpace
       << simulcast.receive_layers();
  }

  return sb.str();
}

// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
// a:simulcast:<send> <streams> <recv> <streams>
// Formal Grammar
// sc-value     = ( sc-send [SP sc-recv] ) / ( sc-recv [SP sc-send] )
// sc-send      = %s"send" SP sc-str-list
// sc-recv      = %s"recv" SP sc-str-list
// sc-str-list  = sc-alt-list *( ";" sc-alt-list )
// sc-alt-list  = sc-id *( "," sc-id )
// sc-id-paused = "~"
// sc-id        = [sc-id-paused] rid-id
// rid-id       = 1*(alpha-numeric / "-" / "_") ; see: I-D.ietf-mmusic-rid
RTCErrorOr<SimulcastDescription> SdpSerializer::DeserializeSimulcastDescription(
    absl::string_view string) const {
  std::vector<std::string> tokens;
  rtc::tokenize(std::string(string), kDelimiterSpaceChar, &tokens);

  if (tokens.size() != 2 && tokens.size() != 4) {
    return ParseError("Must have one or two <direction, streams> pairs.");
  }

  bool bidirectional = tokens.size() == 4;  // indicates both send and recv

  // Tokens 0, 2 (if exists) should be send / recv
  if ((tokens[0] != kSendDirection && tokens[0] != kReceiveDirection) ||
      (bidirectional && tokens[2] != kSendDirection &&
       tokens[2] != kReceiveDirection) ||
      (bidirectional && tokens[0] == tokens[2])) {
    return ParseError("Valid values: send / recv.");
  }

  // Tokens 1, 3 (if exists) should be alternative layer lists
  RTCErrorOr<SimulcastLayerList> list1, list2;
  list1 = ParseSimulcastLayerList(tokens[1]);
  if (!list1.ok()) {
    return list1.MoveError();
  }

  if (bidirectional) {
    list2 = ParseSimulcastLayerList(tokens[3]);
    if (!list2.ok()) {
      return list2.MoveError();
    }
  }

  // Set the layers so that list1 is for send and list2 is for recv
  if (tokens[0] != kSendDirection) {
    std::swap(list1, list2);
  }

  // Set the layers according to which pair is send and which is recv
  // At this point if the simulcast is unidirectional then
  // either |list1| or |list2| will be in 'error' state indicating that
  // the value should not be used.
  SimulcastDescription simulcast;
  if (list1.ok()) {
    simulcast.send_layers() = list1.MoveValue();
  }

  if (list2.ok()) {
    simulcast.receive_layers() = list2.MoveValue();
  }

  return std::move(simulcast);
}

std::string SdpSerializer::SerializeRidDescription(
    const RidDescription& rid_description) const {
  RTC_DCHECK(!rid_description.rid.empty());
  RTC_DCHECK(rid_description.direction == cricket::kRidDirectionSend ||
             rid_description.direction == cricket::kRidDirectionReceive);

  rtc::StringBuilder builder;
  builder << rid_description.rid << kDelimiterSpace
          << (rid_description.direction == cricket::kRidDirectionSend
                  ? kSendDirection
                  : kReceiveDirection);

  const auto& formats = rid_description.formats;
  const auto& restrictions = rid_description.restrictions;
  if (formats.empty() && restrictions.empty()) {
    return builder.str();
  }

  builder << kDelimiterSpace;

  // Serialize any codecs in the description.
  if (!formats.empty()) {
    builder << kPayloadType << kDelimiterEqual << formats.front();
    // Start the loop at the second element. We are guaranteed that there
    // is at least one element, so the iter != end condition will work.
    for (auto iter = ++formats.begin(); iter != formats.end(); iter++) {
      builder << kDelimiterComma << *iter;
    }
  }

  // Serialize any restrictions in the description.
  if (!restrictions.empty()) {
    bool first_property = formats.empty();  // For the ';' delimiter.
    for (auto iter = restrictions.begin(); iter != restrictions.end(); iter++) {
      if (!first_property) {
        builder << kDelimiterSemicolon;
      }
      first_property = false;

      // Serialize key=val pairs.
      // =val part is ommitted if val is empty.
      builder << iter->first;
      if (!iter->second.empty()) {
        builder << kDelimiterEqual << iter->second;
      }
    }
  }

  return builder.str();
}

// https://tools.ietf.org/html/draft-ietf-mmusic-rid-15#section-10
// Formal Grammar
// rid-syntax         = %s"a=rid:" rid-id SP rid-dir
//                      [ rid-pt-param-list / rid-param-list ]
// rid-id             = 1*(alpha-numeric / "-" / "_")
// rid-dir            = %s"send" / %s"recv"
// rid-pt-param-list  = SP rid-fmt-list *( ";" rid-param )
// rid-param-list     = SP rid-param *( ";" rid-param )
// rid-fmt-list       = %s"pt=" fmt *( "," fmt )
// rid-param          = 1*(alpha-numeric / "-") [ "=" param-val ]
// param-val          = *( %x20-58 / %x60-7E )
//                      ; Any printable character except semicolon
RTCErrorOr<RidDescription> SdpSerializer::DeserializeRidDescription(
    absl::string_view string) const {
  std::vector<std::string> tokens;
  rtc::tokenize(std::string(string), kDelimiterSpaceChar, &tokens);
  if (tokens.size() < 2) {
    return ParseError("Rid Description must contain <rid> <direction>.");
  }

  if (tokens.size() > 3) {
    return ParseError("Invalid Rid Description format. Too many arguments.");
  }

  RidDescription rid_description;

  // The first entry in the tokens is the rid id.
  rid_description.rid = tokens[0];

  // The second entry in the tokens is the direction
  if (tokens[1] != kSendDirection && tokens[1] != kReceiveDirection) {
    return ParseError("Invalid Rid direction. Supported values: send / recv.");
  }

  rid_description.direction = tokens[1] == kSendDirection
                                  ? cricket::kRidDirectionSend
                                  : cricket::kRidDirectionReceive;

  // If there is a third argument it is a format list and/or restriction list.
  if (tokens.size() == 3) {
    std::vector<std::string> restrictions;
    rtc::tokenize(tokens[2], kDelimiterSemicolonChar, &restrictions);

    // Check for malformed restriction list, such as ';' or ';;;' etc.
    if (restrictions.empty()) {
      return ParseError("Invalid Rid restriction list: " + tokens[2]);
    }

    // Parse the restrictions. The format indicator (pt) can only appear first.
    for (const std::string& restriction : restrictions) {
      std::vector<std::string> parts;
      rtc::tokenize(restriction, kDelimiterEqualChar, &parts);
      if (parts.empty() || parts.size() > 2) {
        return ParseError("Invalid format for restriction: " + restriction);
      }

      // |parts| contains at least one value and it does not contain a space.
      // Note: |parts| and other values might still contain tab, newline,
      // unprintable characters, etc. which will not generate errors here but
      // will (most-likely) be ignored by components down stream.
      if (parts[0] == kPayloadType) {
        RTCError error = ParseRidFormatList(
            parts.size() > 1 ? parts[1] : std::string(), &rid_description);
        if (!error.ok()) {
          return std::move(error);
        }

        continue;
      }

      // Parse |parts| as a key=value pair which allows unspecified values.
      if (rid_description.restrictions.find(parts[0]) !=
          rid_description.restrictions.end()) {
        return ParseError("Duplicate restriction specified: " + parts[0]);
      }

      rid_description.restrictions[parts[0]] =
          parts.size() > 1 ? parts[1] : std::string();
    }
  }

  return std::move(rid_description);
}

}  // namespace webrtc
