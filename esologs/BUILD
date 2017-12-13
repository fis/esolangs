cc_binary(
    name = "esologs",
    srcs = [
        "format.cc",
        "format.h",
        "index.cc",
        "index.h",
        "server.cc",
    ],
    deps = [
        ":config_cc_proto",
        ":log_cc_proto",
        "@civetweb//:civetweb",
        "@com_googlesource_code_re2//:re2",
        "@zemlib//base",
        "@zemlib//proto:brotli",
        "@zemlib//proto:delim",
    ],
    linkopts = ["-lstdc++fs"],
)

cc_binary(
    name = "logcat",
    srcs = ["logcat.cc"],
    deps = [
        ":log_cc_proto",
        "@zemlib//proto:brotli",
        "@zemlib//proto:delim",
    ],
)

cc_library(
    name = "writer",
    srcs = ["writer.cc"],
    hdrs = ["writer.h"],
    deps = [
        ":log_cc_proto",
        "@zemlib//proto:delim",
    ],
    linkopts = ["-lstdc++fs"],
    visibility = ["//esobot:__pkg__"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)

proto_library(
    name = "log_proto",
    srcs = ["log.proto"],
)

cc_proto_library(
    name = "log_cc_proto",
    deps = [":log_proto"],
)
