FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    clang-17 lld-17 nasm make cmake \
    qemu-system-x86 mtools xorriso \
    curl git gcc g++ dosfstools gdisk

# Pin Rust nightly
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN rustup install nightly-2024-11-01
RUN rustup default nightly-2024-11-01
RUN rustup target add x86_64-unknown-none

# Pin TinyGo
RUN curl -L https://github.com/tinygo-org/tinygo/releases/download/v0.33.0/tinygo_0.33.0_amd64.deb -o tinygo.deb && \
    dpkg -i tinygo.deb && rm tinygo.deb

# Pin GraalVM
RUN curl -L https://download.oracle.com/graalvm/21/latest/graalvm-jdk-21_linux-x64_bin.tar.gz | tar xz -C /opt
ENV JAVA_HOME=/opt/graalvm-jdk-21
ENV PATH="${JAVA_HOME}/bin:${PATH}"

ENV SOURCE_DATE_EPOCH=1700000000
WORKDIR /build
