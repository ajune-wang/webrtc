# Fuzzing in WebRTC

## Compiling locally
To build the fuzzers residing in the [test/fuzzers][fuzzers] directory, use
```
$ gn gen out/fuzzers --args='use_libfuzzer=true optimize_for_fuzzing=true'
```
Depending on the fuzzer additional arguments like `is_asan`, `is_msan` or `is_ubsan_security` might be required.

See the [GN][gn-doc] documentation for all available options. There are also more
platform specific tips on the [Android][webrtc-android-development] and
[iOS][webrtc-ios-development] instructions.

## Add new fuzzers
Create a new `.cc` file in the [test/fuzzers][test/fuzzers] directory, use existing files as a guide.

Add a new `webrtc_fuzzers_test` build rule in the [test/fuzzers/BUILD.gn][fuzzers/BUILD.gn], use existing rules as a guide.

Ensure it compiles locally then add it to a gerrit CL and upload it for review, e.g.

```
$ autoninja -C out/fuzzers test/fuzzers:h264_depacketizer_fuzzer
```

It can then be executed like so:
```
$ out/fuzzers/bin/run_h264_depacketizer_fuzzer
```

## Running fuzzers automatically
All fuzzer tests in the [test/fuzzers/BUILD.gn][test/fuzzers/BUILD.gn] file are compiled per CL on the [libfuzzer bot][libfuzzer-bot].
https://clusterfuzz.com traverses the coddebase and executes fuzzer targets automatically.

Bugs are filed automatically and assigned based on [test/fuzzers/OWNERS][test/fuzzers/OWNERS] file or the commit history.
If you are a non-googler, you can only view data from https://clusterfuzz.com if your account is CC'ed on the reported bug.

## Additional reading

[Libfuzzer in Chromium][libfuzzer-chromium]


[libfuzzer-chromium]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md
[libfuzzer-bot]: https://ci.chromium.org/ui/p/webrtc/builders/luci.webrtc.ci/Linux64%20Release%20%28Libfuzzer%29 

