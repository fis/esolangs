workspace(name = "esowiki")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "fi_zem_bracket",
    urls = ["https://github.com/fis/bracket/archive/ffcfbcc940457bf9db39ac92e212d1917736f8b6.zip"],
    strip_prefix = "bracket-ffcfbcc940457bf9db39ac92e212d1917736f8b6",
    sha256 = "2d7925e6c253e164dff69fcc732108a336ddf123b86199dd2a30c970dab50e3a",
)

# For development use:
#local_repository(name = "fi_zem_bracket", path = "../bracket")

load("@fi_zem_bracket//:repositories.bzl", "bracket_repositories")
bracket_repositories()

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
