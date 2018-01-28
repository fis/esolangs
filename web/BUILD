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
        "request.h",
        "response.h",
        "server.h",
        "websocket.h",
        "writer.h",
    ],
    deps = [
        "@civetweb//:civetweb",
        "@fi_zem_bracket//base",
    ],
)
