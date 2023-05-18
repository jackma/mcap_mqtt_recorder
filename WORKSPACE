workspace(name = "mcap_mqtt_recorder")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "mcap",
    build_file_content = """
package(
    default_visibility = [
        "//visibility:public",
    ],
)

cc_library(
    name = "mcap",
    hdrs = glob(["mcap-releases-cpp-v1.0.0/cpp/mcap/include/mcap/*.hpp", "mcap-releases-cpp-v1.0.0/cpp/mcap/include/mcap/*.inl"]),
    strip_include_prefix = "mcap-releases-cpp-v1.0.0/cpp/mcap/include",
    includes = ["mcap-releases-cpp-v1.0.0/cpp/mcap/include"],
    visibility = ["//visibility:public"],
)
""",
    sha256 = "e36169e46a67a9431f73df335f67488461817bc423f9af63ac0af7f29e0bd696",
    urls = ["https://github.com/foxglove/mcap/archive/refs/tags/releases/cpp/v1.0.0.tar.gz"],
)
