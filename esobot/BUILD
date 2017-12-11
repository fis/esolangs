cc_binary(
    name = "esobot",
    srcs = [
        "esobot.cc",
        "logger.cc",
        "logger.h",
        "rcfeed.cc",
        "rcfeed.h",
    ],
    deps = [
        ":config_cc_proto",
        "//esologs:writer",
        "@zemlib//base",
        "@zemlib//event",
        "@zemlib//irc",
        "@zemlib//irc:config_cc_proto",
        "@zemlib//proto:util",
    ],
    linkopts = ["-lstdc++fs"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = ["@zemlib//irc:config_proto"],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)
