package(default_visibility = ["//visibility:public"])

load("@esowiki//webgen:closure.bzl", "closure_targets")
closure_targets()

filegroup(
    name = "erb",
    srcs = ["erb.rb"],
)

py_binary(
    name = "archive",
    srcs = ["archive.py"],
)

py_library(
    name = "preview",
    srcs = ["preview.py"],
    deps = ["@fi_zem_bracket//tools:runfiles"],
)

filegroup(
    name = "preview_stub",
    srcs = ["preview_stub.py"],
)
