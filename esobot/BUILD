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
        "@com_googlesource_code_re2//:re2",
        "@fi_zem_bracket//base",
        "@fi_zem_bracket//irc",
        "@fi_zem_bracket//irc/bot",
    ],
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
