"""
External non-module dependencies for the esolangs project.

These are imported into the Bzlmod module system via the extensions.bzl file.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

def esolangs_repositories():
    if not native.existing_rule("closure_compiler_prebuilt"):
        esolangs_dep_closure_compiler_prebuilt()
    if not native.existing_rule("com_github_yhirose_cpp_httplib"):
        esolangs_dep_cpp_httplib()

# closure-compiler (20180204)

def esolangs_dep_closure_compiler_prebuilt():
    http_file(
        name = "closure_compiler_prebuilt",
        url = "http://dl.google.com/closure-compiler/compiler-20180204.zip",
        sha256 = "67374a621c663b416fa462b6fd9f5b4a5a753b44eeb914413a47d3312323c857",
    )

# yhirose/cpp-httplib [testing only]

def esolangs_dep_cpp_httplib():
    http_archive(
        name = "com_github_yhirose_cpp_httplib",
        urls = ["https://github.com/yhirose/cpp-httplib/archive/75fdb06696d4e27b590135c1e4a226b4e13e9bd7.zip"],
        strip_prefix = "cpp-httplib-75fdb06696d4e27b590135c1e4a226b4e13e9bd7",
        sha256 = "1393c3b4b47b060fb11d669ffb1c74f2617a98163309c59b8522f15501d96a2a",
        build_file = "@esolangs//tools:com_github_yhirose_cpp_httplib.BUILD",
    )

# 
