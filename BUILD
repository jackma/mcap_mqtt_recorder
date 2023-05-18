cc_binary(
    name = "mcap_writer",
    srcs = [
        "main.cpp",
    ],
    deps = [
        "@mcap",
    ],
    linkopts = [
        "-llz4",
        "-lzstd",
        "-lpaho-mqttpp3",
        "-lpaho-mqtt3a",
    ]
)

