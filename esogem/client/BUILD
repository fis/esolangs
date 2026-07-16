load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "client",
    srcs = ["main.cc"],
    deps = [
        "@bracket//base",
        "@bracket//event",
        "@bracket//http",
        "@bracket//irc/bot:remote_service_brpc",
        "@bracket-protos//irc/bot:remote_service_cc_proto",
    ],
)
