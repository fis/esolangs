workspace(name = "esowiki")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "fi_zem_bracket",
    urls = ["https://github.com/fis/bracket/archive/26000aa6c74edabd51909aab26b9fee222d472f1.zip"],
    strip_prefix = "bracket-26000aa6c74edabd51909aab26b9fee222d472f1",
    sha256 = "3d08113b9cc35096c00d27331c78338cbe2f84265613c8974df04cf6b4d33479",
)

# For development use:
#local_repository(name = "fi_zem_bracket", path = "../bracket")

load("@fi_zem_bracket//:repositories.bzl", "bracket_repositories")
bracket_repositories()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

http_archive(
    name = "com_github_yhirose_cpp_httplib",
    urls = ["https://github.com/yhirose/cpp-httplib/archive/75fdb06696d4e27b590135c1e4a226b4e13e9bd7.zip"],
    strip_prefix = "cpp-httplib-75fdb06696d4e27b590135c1e4a226b4e13e9bd7",
    sha256 = "1393c3b4b47b060fb11d669ffb1c74f2617a98163309c59b8522f15501d96a2a",
    build_file = "//tools:com_github_yhirose_cpp_httplib.BUILD",
)

http_archive(
    name = "com_googlesource_code_re2",
    urls = ["https://github.com/google/re2/archive/2017-12-01.tar.gz"],
    strip_prefix = "re2-2017-12-01",
    sha256 = "62797e7cd7cc959419710cd25b075b5f5b247da0e8214d47bf5af9b32128fb0d",
)

load("@esowiki//webgen:closure.bzl", "closure_compiler_repository")
closure_compiler_repository()
