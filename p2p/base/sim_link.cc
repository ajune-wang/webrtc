#include "p2p/base/sim_link.h"

#include <linux/if.h>
#include <sys/socket.h>
#include <utility>

#include "p2p/base/sim_interface.h"
#include "p2p/base/sim_packet.h"
#include "p2p/base/stun.h"

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"

namespace webrtc {

SimLink::SimLink(rtc::Thread* nio_thread) : nio_thread_(nio_thread) {}
SimLink::~SimLink() = default;

BasicPointToPointLink::Builder::Builder(rtc::Thread* nio_thread,
                                        SimInterface* iface1,
                                        SimInterface* iface2)
    : nio_thread_(nio_thread), iface1_(iface1), iface2_(iface2) {
  RTC_DCHECK(nio_thread_ != nullptr);
}
BasicPointToPointLink::Builder::~Builder() = default;

BasicPointToPointLink::Builder& BasicPointToPointLink::Builder::SetBandwidth(
    absl::optional<uint32_t> bw_bps) {
  bw_bps_ = bw_bps;
  return *this;
}
BasicPointToPointLink::Builder&
BasicPointToPointLink::Builder::SetPacketDropProbability(
    absl::optional<double> drop_prob) {
  drop_prob_ = drop_prob;
  return *this;
}
std::unique_ptr<SimLink> BasicPointToPointLink::Builder::Build() {
  return absl::WrapUnique(new BasicPointToPointLink(
      nio_thread_, iface1_, iface2_, bw_bps_, drop_prob_));
}

BasicPointToPointLink::BasicPointToPointLink(rtc::Thread* nio_thread,
                                             SimInterface* iface1,
                                             SimInterface* iface2,
                                             absl::optional<uint32_t> bw_bps,
                                             absl::optional<double> drop_prob)
    : SimLink(nio_thread),
      iface1_(iface1),
      iface2_(iface2),
      bw_bps_(bw_bps),
      drop_prob_(drop_prob) {}

BasicPointToPointLink::~BasicPointToPointLink() = default;

void BasicPointToPointLink::OnPacketReceived(
    rtc::scoped_refptr<SimPacket> packet,
    const rtc::SocketAddress& src_addr,
    SimInterface* dst_iface,
    int dst_port) {
  RTC_DCHECK_RUN_ON(nio_thread_);
  RTC_DCHECK(dst_iface == iface1_ || dst_iface == iface2_);
  auto* src_this_link = (dst_iface == iface1_ ? iface2_ : iface1_);
  if (src_this_link->ip() != src_addr.ipaddr()) {
    // This packet should not be processed by this link.
    return;
  }
  SignalPacketReadyToReplay(packet, src_this_link, src_addr.port(), dst_iface,
                            dst_port);
}

}  // namespace webrtc
