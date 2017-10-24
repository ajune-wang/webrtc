#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: run.sh ZX1G523SRN"
    exit 1
fi

source settings.sh

WEBRTC_DIR=$HOME/src/webrtc/src
BUILD_DIR=$WEBRTC_DIR/out/Android_Release
ADB=`which adb`  # Explicitly don't use the third_party adb.
SERIAL=$1
TIMEOUT=7200  # Two hours.

# Ensure we are using the latest version.
ninja -C $BUILD_DIR modules_tests

# Transfer the required files.
$WEBRTC_DIR/build/android/test_runner.py gtest \
  --output-directory $BUILD_DIR \
  --suite modules_tests \
  --gtest_filter "DoesNotExist" \
  --shard-timeout $TIMEOUT \
  --runtime-deps-path $BUILD_DIR/gen.runtime/modules/modules_tests__test_runner_script.runtime_deps \
  --adb-path $ADB \
  --device $SERIAL \
  --verbose

# Run all tests as separate test invocations.
mkdir $SERIAL
pushd $SERIAL
mkdir recordings
for clip in "${CLIPS[@]}"; do
  for resolution in "${RESOLUTIONS[@]}"; do
    for framerate in "${FRAMERATES[@]}"; do
      test_name=${clip}_r${resolution}_f${framerate}
      log_name=${test_name}.log

      $WEBRTC_DIR/build/android/test_runner.py gtest \
        --output-directory $BUILD_DIR \
        --suite modules_tests \
        --gtest_filter "CodecSettings/*${test_name}*" \
        --shard-timeout $TIMEOUT \
        --runtime-deps-path ../empty-runtime-deps \
        --test-launcher-retry-limit 0 \
        --adb-path $ADB \
        --device $SERIAL \
        --verbose \
        2>&1 | tee ${log_name}

        if [ $PULL_RECORDINGS -eq 1 ]; then
          pushd recordings
          adb -s $SERIAL shell "ls /sdcard/chromium_tests_root/${test_name}*.ivf" | tr -d '\r' | xargs -n1 adb -s $SERIAL pull 2>&1 >/dev/null
          popd
        fi
    done
  done
done
popd
