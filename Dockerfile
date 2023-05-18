FROM ubuntu:22.04

# Install dependencies
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    software-properties-common \
    libpaho-mqttpp-dev \
    libpaho-mqtt-dev \
    mosquitto \
    mosquitto-clients \
    liblz4-dev \
    libzstd-dev \
    wget \
    clang-15 \
    libcli11-dev

# Install Bazel
RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.17.0/bazelisk-linux-amd64 && \
    cp bazelisk-linux-amd64 /usr/local/bin/bazel && chmod +x /usr/local/bin/bazel

COPY . /app
WORKDIR /app
RUN bazel build :mcap_writer
ENTRYPOINT ["/app/bazel-bin/mcap_writer"]
