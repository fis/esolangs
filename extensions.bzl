"""Module extension for non-module dependencies of the esolangs project."""

load(
    "//:repositories.bzl",
    "esolangs_dep_closure_compiler_prebuilt",
    "esolangs_dep_cpp_httplib",
)

def _non_module_dependencies_impl(_ctx):
    esolangs_dep_closure_compiler_prebuilt()
    esolangs_dep_cpp_httplib()

non_module_dependencies = module_extension(
    implementation = _non_module_dependencies_impl,
)
