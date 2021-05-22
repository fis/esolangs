workspace(name = "esowiki")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "fi_zem_bracket",
    urls = ["https://github.com/fis/bracket/archive/c342cff798a9198b9c6275c7b6bfa53195b95c06.zip"],
    strip_prefix = "bracket-c342cff798a9198b9c6275c7b6bfa53195b95c06",
    sha256 = "54d75e6e5ca30e0fd89a2cc728fa0fb0003ea0f84ec15b204d8fb2fed94d8abf",
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

http_archive(
    name = "hinnant_date",
    urls = ["https://github.com/HowardHinnant/date/archive/0af76547641987bc736d659b4eaf7cd720028288.zip"],
    strip_prefix = "date-0af76547641987bc736d659b4eaf7cd720028288",
    sha256 = "1449fa9355340fe3f5bac103d0ec28f49172a7e6c91bf6611b4e8e2e4ac6dcd0",
    build_file = "//tools:hinnant_date.BUILD",
)

load("@esowiki//webgen:closure.bzl", "closure_compiler_repository")
closure_compiler_repository()
