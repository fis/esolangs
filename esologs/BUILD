load("@fi_zem_bracket//tools:gtest.bzl", "cc_gtest")

cc_binary(
    name = "esologs",
    srcs = ["esologs.cc"],
    deps = [
        ":server",
        "@fi_zem_bracket//event",
        "@fi_zem_bracket//proto:util",
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
        "@com_googlesource_code_re2//:re2",
        "@fi_zem_bracket//base",
        "@fi_zem_bracket//event",
        "@fi_zem_bracket//proto:brotli",
        "@fi_zem_bracket//proto:delim",
        "@hinnant_date//:date",
        "@com_github_jupp0r_prometheus_cpp//core",
        "@com_github_jupp0r_prometheus_cpp//pull",
    ],
    linkopts = ["-lstdc++fs"],
)

cc_gtest(
    name = "server_test",
    deps = [
        ":server",
        "@com_github_yhirose_cpp_httplib//:httplib",
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
        "@fi_zem_bracket//base",
        "@fi_zem_bracket//event",
        "@fi_zem_bracket//proto:delim",
        "@fi_zem_bracket//proto:util",
        "@hinnant_date//:date",
        "@com_github_jupp0r_prometheus_cpp//core",
    ],
    linkopts = ["-lstdc++fs"],
    visibility = ["//esobot:__pkg__"],
)

cc_binary(
    name = "logcat",
    srcs = ["logcat.cc"],
    deps = [
        ":log_cc_proto",
        "@fi_zem_bracket//proto:brotli",
        "@fi_zem_bracket//proto:delim",
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
