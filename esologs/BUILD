load("@bracket//tools:gtest.bzl", "cc_gtest")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

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
        "verifier.cc",
        "verifier.h",
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
        "@prometheus-cpp//core",
        "@prometheus-cpp//pull",
        "@re2//:re2",
    ],
    linkopts = ["-lstdc++fs"],
)

cc_gtest(
    name = "server_test",
    deps = [
        ":server",
        "@cpp-httplib//:httplib",
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
        "@prometheus-cpp//core",
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
