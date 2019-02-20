#include "rtc_base/foo.h"

#include "absl/base/attributes.h"
#include "rtc_base/checks.h"

namespace rtc {

namespace {
ABSL_CONST_INIT thread_local bool thread_may_block = true;
}

void ThreadMayBlock(bool may) {
  RTC_CHECK(thread_may_block);
  RTC_CHECK(!may);
  thread_may_block = may;
}

bool ThreadMayBlock() {
  return thread_may_block;
}

}  // namespace rtc
