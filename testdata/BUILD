filegroup(
    name = "test_logs",
    srcs = glob([
        "logs/**/*.pb",
        "logs/**/*.pb.br",
    ]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "golden_esologs",
    srcs = glob(["golden/esologs.*"]),
    visibility = ["//visibility:public"],
)
