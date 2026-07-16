load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "esobot",
    srcs = [
        "bridge.cc",
        "bridge.h",
        "esobot.cc",
        "logger.cc",
        "logger.h",
        "rcfeed.cc",
        "rcfeed.h",
    ],
    deps = [
        ":config_cc_proto",
        "//esologs:writer",
        "@bracket//base",
        "@bracket//irc",
        "@bracket//irc/bot",
        "@bracket//irc/bot:remote",
        "@re2//:re2",
    ],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "@bracket//irc:config_proto",
        "@bracket//irc/bot:config_proto",
        "@bracket-protos//irc/bot:remote_service_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)
