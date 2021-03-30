package(
    default_copts = ["-Wc++14-compat"],
    default_hdrs_check = "strict",
    default_visibility = [
        "//media/webrtc/media_router:media_router_core",
    ],
)

cc_library(
    name = "chunk",
    srcs = [
        "abort_chunk.cc",
        "chunk.cc",
        "cookie_ack_chunk.cc",
        "cookie_echo_chunk.cc",
        "data_chunk.cc",
        "error_chunk.cc",
        "forward_tsn_chunk.cc",
        "heartbeat_ack_chunk.cc",
        "heartbeat_request_chunk.cc",
        "idata_chunk.cc",
        "iforward_tsn_chunk.cc",
        "init_ack_chunk.cc",
        "init_chunk.cc",
        "reconfig_chunk.cc",
        "sack_chunk.cc",
        "shutdown_ack_chunk.cc",
        "shutdown_chunk.cc",
        "shutdown_complete_chunk.cc",
    ],
    hdrs = [
        "abort_chunk.h",
        "chunk.h",
        "cookie_ack_chunk.h",
        "cookie_echo_chunk.h",
        "data_chunk.h",
        "data_common.h",
        "error_chunk.h",
        "forward_tsn_chunk.h",
        "forward_tsn_common.h",
        "heartbeat_ack_chunk.h",
        "heartbeat_request_chunk.h",
        "idata_chunk.h",
        "iforward_tsn_chunk.h",
        "init_ack_chunk.h",
        "init_chunk.h",
        "reconfig_chunk.h",
        "sack_chunk.h",
        "shutdown_ack_chunk.h",
        "shutdown_chunk.h",
        "shutdown_complete_chunk.h",
    ],
    deps = [
        "//base:logging",
        "//net/dcsctp/common:math",
        "//net/dcsctp/common:str_join",
        "//net/dcsctp/packet:bounded_io",
        "//net/dcsctp/packet:data",
        "//net/dcsctp/packet:tlv_trait",
        "//net/dcsctp/packet/error_cause",
        "//net/dcsctp/packet/parameter",
        "//absl/algorithm:container",
        "//absl/memory",
        "//absl/strings",
        "//absl/types:optional",
        "//api:array_view",
        "//third_party/webrtc/files/stable/webrtc/modules/rtp_rtcp:rtp_rtcp_format",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:logging",
        "//third_party/webrtc/files/stable/webrtc/rtc_base:stringutils.headers",
    ],
)

cc_test(
    name = "sack_chunk_test",
    srcs = ["sack_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "heartbeat_request_chunk_test",
    srcs = ["heartbeat_request_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/parameter",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "iforward_tsn_chunk_test",
    srcs = ["iforward_tsn_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "reconfig_chunk_test",
    srcs = ["reconfig_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet:tlv_trait",
        "//net/dcsctp/packet/parameter",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "shutdown_chunk_test",
    srcs = ["shutdown_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "init_ack_chunk_test",
    srcs = ["init_ack_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/parameter",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "heartbeat_ack_chunk_test",
    srcs = ["heartbeat_ack_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/parameter",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "abort_chunk_test",
    srcs = ["abort_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "error_chunk_test",
    srcs = ["error_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "shutdown_complete_chunk_test",
    srcs = ["shutdown_complete_chunk_test.cc"],
    deps = [
        ":chunk",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "cookie_ack_chunk_test",
    srcs = ["cookie_ack_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "idata_chunk_test",
    srcs = ["idata_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "cookie_echo_chunk_test",
    srcs = ["cookie_echo_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "forward_tsn_chunk_test",
    srcs = ["forward_tsn_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "shutdown_ack_chunk_test",
    srcs = ["shutdown_ack_chunk_test.cc"],
    deps = [
        ":chunk",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "data_chunk_test",
    srcs = ["data_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "init_chunk_test",
    srcs = ["init_chunk_test.cc"],
    deps = [
        ":chunk",
        "//net/dcsctp/packet/parameter",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)
