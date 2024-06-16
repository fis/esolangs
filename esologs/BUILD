load("@bracket//tools:gtest.bzl", "cc_gtest")

cc_binary(
    name = "esologs",
    srcs = ["esologs.cc"],
    deps = [
        ":server",
        "@bracket//event",
        "@bracket//proto:util",
    ],
)

cc_library(
    name = "server",
    srcs = [
        "format.cc",
        "format.h",
        "index.cc",
        "index.h",
        "server.cc",
        "stalker.cc",
        "stalker.h",
    ],
    hdrs = [
        "server.h",
    ],
    deps = [
        ":config_cc_proto",
        ":log_cc_proto",
        "//web",
        "@bracket//base",
        "@bracket//event",
        "@bracket//proto:brotli",
        "@bracket//proto:delim",
        "@hinnant_date//:date",
        "@prometheus_cpp//core",
        "@prometheus_cpp//pull",
        "@re2//:re2",
    ],
    linkopts = ["-lstdc++fs"],
)

cc_gtest(
    name = "server_test",
    deps = [
        ":server",
        "@cpp_httplib//:httplib",
    ],
    data = [
        "//testdata:golden_esologs",
        "//testdata:test_logs",
    ],
)

cc_library(
    name = "writer",
    srcs = ["writer.cc"],
    hdrs = ["writer.h"],
    deps = [
        ":config_cc_proto",
        ":log_cc_proto",
        "@bracket//base",
        "@bracket//event",
        "@bracket//proto:delim",
        "@bracket//proto:util",
        "@hinnant_date//:date",
        "@prometheus_cpp//core",
    ],
    linkopts = ["-lstdc++fs"],
    visibility = ["//esobot:__pkg__"],
)

cc_binary(
    name = "logcat",
    srcs = ["logcat.cc"],
    deps = [
        ":log_cc_proto",
        "@bracket//proto:brotli",
        "@bracket//proto:delim",
    ],
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
