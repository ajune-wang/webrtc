#ifndef SIMLINK_H_
#define SIMLINK_H_

#include <memory>

#include "p2p/base/sim_interface.h"
#include "p2p/base/sim_packet.h"

#include "absl/types/optional.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace webrtc {

class SimLink : public ::sigslot::has_slots<> {
 public:
  SimLink(rtc::Thread* nio_thread);
  ~SimLink() override;
  enum class Type {
    kPointToPoint,
    // kBroadcast,
  };

  virtual void OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                                const rtc::SocketAddress& src_addr,
                                SimInterface* dst_iface,
                                int dst_port) = 0;

  sigslot::signal5<rtc::scoped_refptr<SimPacket>, /* packet */
                   SimInterface*,                 /* src_iface */
                   int,                           /* src_port */
                   SimInterface*,                 /* dst_iface */
                   int /* dst_port */>
      SignalPacketReadyToReplay;

 protected:
  rtc::Thread* nio_thread_;
};

class BasicPointToPointLink : public SimLink {
 public:
  ~BasicPointToPointLink() override;

  class Builder {
   public:
    Builder(rtc::Thread* nio_thread,
            SimInterface* iface1,
            SimInterface* iface2);
    ~Builder();
    Builder& SetBandwidth(absl::optional<uint32_t> bw_bps);
    Builder& SetPacketDropProbability(absl::optional<double> drop_prob);
    std::unique_ptr<SimLink> Build();

   private:
    rtc::Thread* nio_thread_;
    SimInterface* iface1_;
    SimInterface* iface2_;
    // A null value represents infinity bandwidth.
    absl::optional<uint32_t> bw_bps_;
    absl::optional<double> drop_prob_;
  };

 private:
  friend Builder;

  BasicPointToPointLink(rtc::Thread* nio_thread,
                        SimInterface* iface1,
                        SimInterface* iface2,
                        absl::optional<uint32_t> bw_bps,
                        absl::optional<double> drop_prob);

  void OnPacketReceived(rtc::scoped_refptr<SimPacket> packet,
                        const rtc::SocketAddress& src_addr,
                        SimInterface* dst_iface,
                        int dst_port) override;

  SimInterface* iface1_;
  SimInterface* iface2_;
  // A null value represents infinity bandwidth.
  absl::optional<uint32_t> bw_bps_;
  absl::optional<double> drop_prob_;
  rtc::AsyncInvoker invoker_;
};

}  // namespace webrtc

#endif  // SIMLINK_H_
