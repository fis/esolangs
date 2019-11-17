"""External dependencies for the Closure Compiler."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

_CLOSURE_COMPILER_URL = "http://dl.google.com/closure-compiler/compiler-20180204.zip"
_CLOSURE_COMPILER_SHA = "67374a621c663b416fa462b6fd9f5b4a5a753b44eeb914413a47d3312323c857"
_CLOSURE_COMPILER_JAR = "closure-compiler-v20180204.jar"

def closure_compiler_repository():
    """Defines closure_compiler_dist as a remote repository for the
    precompiled Closure Compiler distribution archive."""

    http_file(
        name = "closure_compiler_dist",
        urls = [_CLOSURE_COMPILER_URL],
        sha256 = _CLOSURE_COMPILER_SHA,
    )

def closure_targets():
    """Defines a java_binary target "closure_compiler" for the Closure Compiler tool."""

    native.genrule(
        name = "closure_compiler_unzip",
        srcs = ["@closure_compiler_dist//file"],
        outs = [_CLOSURE_COMPILER_JAR],
        cmd = "unzip -jDD $< %s -d $$(dirname $@)" % _CLOSURE_COMPILER_JAR,
    )
    native.java_import(
        name = "closure_compiler_jar",
        jars = [":closure_compiler_unzip"],
    )
    native.java_binary(
        name = "closure_compiler",
        runtime_deps = [":closure_compiler_jar"],
        main_class = "com.google.javascript.jscomp.CommandLineRunner",
        visibility = ["//visibility:public"],
    )
