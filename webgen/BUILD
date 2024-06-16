package(default_visibility = ["//visibility:public"])

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
    deps = ["@bracket//tools:runfiles"],
)

filegroup(
    name = "preview_stub",
    srcs = ["preview_stub.py"],
)

# targets for the prebuilt Closure Compiler tool

genrule(
    name = "closure_compiler_unzip",
    srcs = ["@closure_compiler_prebuilt//file"],
    outs = ["closure-compiler-v20180204.jar"],
    cmd = "unzip -jDD $< closure-compiler-v20180204.jar -d $$(dirname $@)",
    visibility = ["//visibility:private"],
)

java_import(
    name = "closure_compiler_jar",
    jars = [":closure_compiler_unzip"],
    visibility = ["//visibility:private"],
)

java_binary(
    name = "closure_compiler",
    runtime_deps = [":closure_compiler_jar"],
    main_class = "com.google.javascript.jscomp.CommandLineRunner",
)
