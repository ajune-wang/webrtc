#include "net/dcsctp/packet/error_cause/cookie_received_while_shutting_down_cause.h"

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.10.10

//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |     Cause Code=10              |      Cause Length=4          |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr int CookieReceivedWhileShuttingDownCause::kType;

absl::optional<CookieReceivedWhileShuttingDownCause>
CookieReceivedWhileShuttingDownCause::Parse(
    rtc::ArrayView<const uint8_t> data) {
  if (!ParseTLV(data).has_value()) {
    return absl::nullopt;
  }
  return CookieReceivedWhileShuttingDownCause();
}

void CookieReceivedWhileShuttingDownCause::SerializeTo(
    std::vector<uint8_t>& out) const {
  AllocateTLV(out);
}

std::string CookieReceivedWhileShuttingDownCause::ToString() const {
  return "Cookie Received While Shutting Down";
}
}  // namespace dcsctp
