load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "web",
    srcs = [
        "server.cc",
        "writer.cc",
    ],
    hdrs = [
        "request.h",
        "response.h",
        "server.h",
        "websocket.h",
        "writer.h",
    ],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_map",
        "@bracket//base",
        "@civetweb//:civetweb",
    ],
)
