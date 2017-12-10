cc_binary(
    name = "esobot",
    srcs = ["esobot.cc"],
    deps = [
        ":log_cc_proto",
        "@zemlib//base",
        "@zemlib//event",
        "@zemlib//irc",
        "@zemlib//irc:config_cc_proto",
    ],
    linkopts = ["-lstdc++fs"],
)

proto_library(
    name = "log_proto",
    srcs = ["log.proto"],
)

cc_proto_library(
    name = "log_cc_proto",
    deps = [":log_proto"],
)
