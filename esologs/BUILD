cc_library(
    name = "writer",
    srcs = ["writer.cc"],
    hdrs = ["writer.h"],
    deps = [
        ":log_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@zemlib//proto:delim",
    ],
    linkopts = ["-lstdc++fs"],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "log_proto",
    srcs = ["log.proto"],
)

cc_proto_library(
    name = "log_cc_proto",
    deps = [":log_proto"],
)
