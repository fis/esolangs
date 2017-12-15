workspace(name = "esowiki")

local_repository(
    name = "fi_zem_bracket",
    path = "../bracket",
)

load("@fi_zem_bracket//:repositories.bzl", "bracket_repositories")
bracket_repositories()

new_http_archive(
    name = "civetweb",
    urls = ["https://github.com/civetweb/civetweb/archive/v1.9.1.tar.gz"],
    strip_prefix = "civetweb-1.9.1",
    sha256 = "880d741724fd8de0ebc77bc5d98fa673ba44423dc4918361c3cd5cf80955e36d",
    build_file = "//tools:BUILD.civetweb",
)

http_archive(
    name = "com_googlesource_code_re2",
    urls = ["https://github.com/google/re2/archive/2017-12-01.tar.gz"],
    strip_prefix = "re2-2017-12-01",
    sha256 = "62797e7cd7cc959419710cd25b075b5f5b247da0e8214d47bf5af9b32128fb0d",
)

new_http_archive(
    name = "hinnant_date",
    urls = ["https://github.com/HowardHinnant/date/archive/0af76547641987bc736d659b4eaf7cd720028288.zip"],
    strip_prefix = "date-0af76547641987bc736d659b4eaf7cd720028288",
    sha256 = "1449fa9355340fe3f5bac103d0ec28f49172a7e6c91bf6611b4e8e2e4ac6dcd0",
    build_file = "//tools:BUILD.hinnant_date",
)
