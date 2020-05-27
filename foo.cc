#include <stdlib.h>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"

void Bar() {
  abort();
}

void Foo() {
  Bar();
}

int main(int argc, char** argv) {
  // Initialize the symbolizer to get a human-readable stack trace
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  absl::InstallFailureSignalHandler(options);
  Foo();
  return 0;
}
