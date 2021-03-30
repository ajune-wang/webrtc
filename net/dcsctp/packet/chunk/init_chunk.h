#ifndef NET_DCSCTP_PACKET_CHUNK_INIT_CHUNK_H_
#define NET_DCSCTP_PACKET_CHUNK_INIT_CHUNK_H_
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/tlv_trait.h"

namespace dcsctp {

// https://tools.ietf.org/html/rfc4960#section-3.3.2
struct InitChunkConfig : ChunkConfig {
  static constexpr int kType = 1;
  static constexpr size_t kHeaderSize = 20;
  static constexpr size_t kVariableLengthAlignment = 1;
};

class InitChunk : public Chunk, public TLVTrait<InitChunkConfig> {
 public:
  static constexpr int kType = InitChunkConfig::kType;

  InitChunk(uint32_t initiate_tag,
            uint32_t a_rwnd,
            uint16_t nbr_outbound_streams,
            uint16_t nbr_inbound_streams,
            uint32_t initial_tsn,
            Parameters parameters)
      : initiate_tag_(initiate_tag),
        a_rwnd_(a_rwnd),
        nbr_outbound_streams_(nbr_outbound_streams),
        nbr_inbound_streams_(nbr_inbound_streams),
        initial_tsn_(initial_tsn),
        parameters_(std::move(parameters)) {}

  InitChunk(InitChunk&& other) = default;
  InitChunk& operator=(InitChunk&& other) = default;

  static absl::optional<InitChunk> Parse(rtc::ArrayView<const uint8_t> data);

  void SerializeTo(std::vector<uint8_t>& out) const override;
  std::string ToString() const override;

  uint32_t initiate_tag() const { return initiate_tag_; }
  uint32_t a_rwnd() const { return a_rwnd_; }
  uint16_t nbr_outbound_streams() const { return nbr_outbound_streams_; }
  uint16_t nbr_inbound_streams() const { return nbr_inbound_streams_; }
  uint32_t initial_tsn() const { return initial_tsn_; }
  const Parameters& parameters() const { return parameters_; }

 private:
  uint32_t initiate_tag_;
  uint32_t a_rwnd_;
  uint16_t nbr_outbound_streams_;
  uint16_t nbr_inbound_streams_;
  uint32_t initial_tsn_;
  Parameters parameters_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_CHUNK_INIT_CHUNK_H_
