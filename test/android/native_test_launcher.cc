/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/android/native_test_launcher.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

#include <android/log.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include <iterator>
#include <string>
#include <vector>

#include "base/android/jni_string.h"
#include "base/base_switches.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/test_support_android.h"
#include "base/threading/thread_restrictions.h"
#include "test/android/native_test_util.h"
#include "test/native_test_jni/NativeTestWebrtc_jni.h"
#include "third_party/jni_zero/jni_zero.h"

using jni_zero::JavaParamRef;

// The main function of the program to be wrapped as a test apk.
extern int main(int argc, char** argv);

namespace webrtc {
namespace test {
namespace android {

namespace {

const char kLogTag[] = "chromium";
const char kCrashedMarker[] = "[ CRASHED      ]\n";

// The list of signals which are considered to be crashes.
const int kExceptionSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1};

struct sigaction g_old_sa[NSIG];

// This function runs in a compromised context. It should not allocate memory.
void SignalHandler(int sig, siginfo_t* info, void* reserved) {
  // Output the crash marker.
  write(STDOUT_FILENO, kCrashedMarker, sizeof(kCrashedMarker) - 1);
  g_old_sa[sig].sa_sigaction(sig, info, reserved);
}

// Writes printf() style string to Android's logger where |priority| is one of
// the levels defined in <android/log.h>.
void AndroidLog(int priority, const char* format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(priority, kLogTag, format, args);
  va_end(args);
}

}  // namespace

static void JNI_NativeTestWebrtc_RunTests(
    JNIEnv* env,
    const JavaParamRef<jstring>& jcommand_line_flags,
    const JavaParamRef<jstring>& jcommand_line_file_path,
    const JavaParamRef<jstring>& jstdout_file_path,
    const JavaParamRef<jobject>& app_context,
    const JavaParamRef<jstring>& jtest_data_dir) {
  fprintf(stderr, "Entered native code (stderr)\n");
  fprintf(stdout, "Entered native code (stdout)\n");

  base::ScopedAllowBlockingForTesting allow;

  // Required for DEATH_TESTS.
  pthread_atfork(nullptr, nullptr, jni_zero::DisableJvmForTesting);

  // Command line initialized basically, will be fully initialized later.
  static const char* const kInitialArgv[] = {"ChromeTestActivity"};
  base::CommandLine::Init(std::size(kInitialArgv), kInitialArgv);

  std::vector<std::string> args;

  const std::string command_line_file_path(
      webrtc::test::android::ASCIIJavaStringToUTF8(env,
                                                   jcommand_line_file_path));
  if (command_line_file_path.empty())
    args.push_back("_");
  else
    ParseArgsFromCommandLineFile(command_line_file_path.c_str(), &args);

  const std::string command_line_flags(
      webrtc::test::android::ASCIIJavaStringToUTF8(env, jcommand_line_flags));
  ParseArgsFromString(command_line_flags, &args);

  std::vector<char*> argv;
  int argc = ArgsToArgv(args, &argv);

  // Fully initialize command line with arguments.
  base::CommandLine::ForCurrentProcess()->AppendArguments(
      base::CommandLine(argc, &argv[0]), false);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string stdout_file_path(
      webrtc::test::android::ASCIIJavaStringToUTF8(env, jstdout_file_path));

  // A few options, such "--gtest_list_tests", will just use printf directly
  // Always redirect stdout to a known file.
  if (freopen(stdout_file_path.c_str(), "a+", stdout) == NULL) {
    AndroidLog(ANDROID_LOG_ERROR, "Failed to redirect stream to file: %s: %s\n",
               stdout_file_path.c_str(), strerror(errno));
    exit(EXIT_FAILURE);
  }
  // TODO(jbudorick): Remove this after resolving crbug.com/726880
  AndroidLog(ANDROID_LOG_INFO, "Redirecting stdout to file: %s\n",
             stdout_file_path.c_str());
  dup2(STDOUT_FILENO, STDERR_FILENO);

  if (command_line.HasSwitch(switches::kWaitForDebugger)) {
    AndroidLog(ANDROID_LOG_VERBOSE,
               "Native test waiting for GDB because flag %s was supplied",
               switches::kWaitForDebugger);
    base::debug::WaitForDebugger(24 * 60 * 60, true);
  }

  base::FilePath test_data_dir(
      webrtc::test::android::ASCIIJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  ScopedMainEntryLogger scoped_main_entry_logger;
  main(argc, &argv[0]);
}

// TODO(nileshagrawal): now that we're using FIFO, test scripts can detect EOF.
// Remove the signal handlers.
void InstallHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO;

  for (unsigned int i = 0; kExceptionSignals[i] != -1; ++i) {
    sigaction(kExceptionSignals[i], &sa, &g_old_sa[kExceptionSignals[i]]);
  }
}

}  // namespace android
}  // namespace test
}  // namespace webrtc
