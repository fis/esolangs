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
        "@fi_zem_bracket//base",
        "@fi_zem_bracket//event",
        "@fi_zem_bracket//irc",
        "@fi_zem_bracket//irc:config_cc_proto",
        "@fi_zem_bracket//proto:util",
    ],
    linkopts = ["-lstdc++fs"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = ["@fi_zem_bracket//irc:config_proto"],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)
