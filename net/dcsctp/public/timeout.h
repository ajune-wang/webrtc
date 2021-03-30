#ifndef NET_DCSCTP_PUBLIC_TIMEOUT_H_
#define NET_DCSCTP_PUBLIC_TIMEOUT_H_

#include <cstdint>

namespace dcsctp {

// A very simple timeout that can be started and stopped. When started,
// it will be given a unique `timeout_id` which should be provided to
// `DcSctpSocket::HandleTimeout` when it expires.
class Timeout {
 public:
  virtual ~Timeout() = default;

  // Called to start time timeout, with the duration in milliseconds as
  // `duration_ms` and with the timeout identifier as `timeout_id`, which - if
  // the timeout expires - shall be provided to `DcSctpSocket::HandleTimeout`.
  virtual void Start(int duration_ms, uint64_t timeout_id) = 0;

  // Called to stop the running timeout.
  virtual void Stop() = 0;

  // Called to restart an already running timeout, with the `duration_ms` and
  // `timeout_id` parameters as described in `Start`. This can be overridden by
  // the implementation to restart it more efficiently.
  virtual void Restart(int duration_ms, uint64_t timeout_id) {
    Stop();
    Start(duration_ms, timeout_id);
  }
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_PUBLIC_TIMEOUT_H_
