package(
    default_copts = ["-Wc++14-compat"],
    default_hdrs_check = "strict",
    default_visibility = [
        "//media/webrtc/media_router:media_router_core",
    ],
)

cc_library(
    name = "error_cause",
    srcs = [
        "cookie_received_while_shutting_down_cause.cc",
        "error_cause.cc",
        "invalid_mandatory_parameter_cause.cc",
        "invalid_stream_identifier_cause.cc",
        "missing_mandatory_parameter_cause.cc",
        "no_user_data_cause.cc",
        "out_of_resource_error_cause.cc",
        "protocol_violation_cause.cc",
        "restart_of_an_association_with_new_address_cause.cc",
        "stale_cookie_error_cause.cc",
        "unrecognized_chunk_type_cause.cc",
        "unrecognized_parameter_cause.cc",
        "unresolvable_address_cause.cc",
        "user_initiated_abort_cause.cc",
    ],
    hdrs = [
        "cookie_received_while_shutting_down_cause.h",
        "error_cause.h",
        "invalid_mandatory_parameter_cause.h",
        "invalid_stream_identifier_cause.h",
        "missing_mandatory_parameter_cause.h",
        "no_user_data_cause.h",
        "out_of_resource_error_cause.h",
        "protocol_violation_cause.h",
        "restart_of_an_association_with_new_address_cause.h",
        "stale_cookie_error_cause.h",
        "unrecognized_chunk_type_cause.h",
        "unrecognized_parameter_cause.h",
        "unresolvable_address_cause.h",
        "user_initiated_abort_cause.h",
    ],
    deps = [
        "//net/dcsctp/common:math",
        "//net/dcsctp/common:str_join",
        "//net/dcsctp/packet:bounded_io",
        "//net/dcsctp/packet:tlv_trait",
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
    name = "user_initiated_abort_cause_test",
    srcs = ["user_initiated_abort_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/packet:tlv_trait",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "protocol_violation_cause_test",
    srcs = ["protocol_violation_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/packet:tlv_trait",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "error_cause_test",
    srcs = ["error_cause_test.cc"],
    deps = [
        ":error_cause",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "invalid_stream_identifier_cause_test",
    srcs = ["invalid_stream_identifier_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "unresolvable_address_cause_test",
    srcs = ["unresolvable_address_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "cookie_received_while_shutting_down_cause_test",
    srcs = ["cookie_received_while_shutting_down_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "no_user_data_cause_test",
    srcs = ["no_user_data_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "unrecognized_chunk_type_cause_test",
    srcs = ["unrecognized_chunk_type_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "invalid_mandatory_parameter_cause_test",
    srcs = ["invalid_mandatory_parameter_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "stale_cookie_error_cause_test",
    srcs = ["stale_cookie_error_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "unrecognized_parameter_cause_test",
    srcs = ["unrecognized_parameter_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "restart_of_an_association_with_new_address_cause_test",
    srcs = ["restart_of_an_association_with_new_address_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)

cc_test(
    name = "out_of_resource_error_cause_test",
    srcs = ["out_of_resource_error_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
    ],
)

cc_test(
    name = "missing_mandatory_parameter_cause_test",
    srcs = ["missing_mandatory_parameter_cause_test.cc"],
    deps = [
        ":error_cause",
        "//net/dcsctp/testing:testing_macros",
        "//testing/base/public:gunit_main",
        "//api:array_view",
    ],
)
