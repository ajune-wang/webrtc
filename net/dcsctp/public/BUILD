package(
    default_hdrs_check = "strict",
    default_visibility = [
        "//media/webrtc/media_router:media_router_core",
    ],
)

cc_library(
    name = "dcsctp_options",
    hdrs = [
        "dcsctp_options.h",
    ],
)

cc_library(
    name = "packet_observer",
    hdrs = [
        "packet_observer.h",
    ],
    deps = ["//third_party/webrtc/files/stable/webrtc/api:array_view"],
)

cc_library(
    name = "dcsctp_socket",
    hdrs = [
        "dcsctp_socket.h",
        "timeout.h",
    ],
    deps = [
        ":dcsctp_message",
        ":packet_observer",
        "//third_party/absl/strings",
        "//third_party/absl/types:optional",
        "//third_party/webrtc/files/stable/webrtc/api:array_view",
    ],
)

cc_library(
    name = "dcsctp_message",
    hdrs = [
        "dcsctp_message.h",
    ],
    deps = ["//third_party/webrtc/files/stable/webrtc/api:array_view"],
)
