package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "web",
    srcs = [
        "server.cc",
        "writer.cc",
    ],
    hdrs = [
        "response.h",
        "server.h",
        "writer.h",
    ],
    deps = [
        "@civetweb//:civetweb",
        "@fi_zem_bracket//base",
    ],
)
