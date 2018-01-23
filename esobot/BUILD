cc_binary(
    name = "esobot",
    srcs = [
        "esobot.cc",
    ],
    deps = [
        ":config_cc_proto",
        "@fi_zem_bracket//base",
        "@fi_zem_bracket//irc",
        "@fi_zem_bracket//irc/bot",
        "@fi_zem_bracket//proto:util",
    ],
)

cc_binary(
    name = "plugins.so",
    srcs = [
        "logger.cc",
        "logger.h",
        "plugins.cc",
        "rcfeed.cc",
        "rcfeed.h",
    ],
    deps = [
        ":config_cc_proto",
        "//esologs:writer",
        "@fi_zem_bracket//irc/bot:plugin",
    ],
    linkshared = 1,
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "@fi_zem_bracket//irc:config_proto",
        "@fi_zem_bracket//irc/bot:config_proto",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    deps = [":config_proto"],
)
