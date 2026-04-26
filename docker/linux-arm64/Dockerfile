# =============================================
# SynthOrbis UNI — Linux ARM64 交叉编译环境
# 基础镜像：Ubuntu 22.04
# 目标：飞腾/鲲鹏 (aarch64-linux-gnu)
# =============================================

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# ── 交叉编译工具链 ──────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libc6-dev-arm64-cross \
    libboost-dev-arm64-cross \
    libboost-filesystem-dev-arm64-cross \
    libboost-system-dev-arm64-cross \
    libboost-locale-dev-arm64-cross \
    libopencc-dev-arm64-cross \
    libmarisa-dev-arm64-cross \
    libyaml-cpp-dev-arm64-cross \
    libxkbfile-dev-arm64-cross \
    libibus-1.0-dev-arm64-cross \
    git \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ── CMake 交叉编译工具链文件 ────────────────────
# librime cmake 需要这些变量
ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ENV CMAKE_SYSROOT=/usr/aarch64-linux-gnu
ENV RIME_TARGET_ARCH=arm64

WORKDIR /workspace/SynthOrbisUNI

CMD ["/bin/bash"]
