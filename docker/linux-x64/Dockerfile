# =============================================
# SynthOrbis UNI — Linux x64 构建环境
# 基础镜像：Ubuntu 22.04
# 用途：编译 librime Linux 版 + ibus-rime/fcitx-rime
# =============================================

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# ── 系统依赖 ──────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libboost-dev \
    libboost-filesystem-dev \
    libboost-system-dev \
    libboost-locale-dev \
    libopencc-dev \
    libmarisa-dev \
    libgtest-dev \
    libgoogle-glog-dev \
    libyaml-cpp-dev \
    libxkbfile-dev \
    libibus-1.0-dev \
    libfcitx5-dev \
    fcitx5-configtool \
    git \
    curl \
    wget \
    ca-certificates \
    unzip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ── CMake / Ninja（更新版本）─────────────────
RUN curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' > /etc/apt/sources.list.d/kitware.list \
    && apt-get update \
    && apt-get install -y cmake ninja-build

# ── RIME 源码目录 ──────────────────────────────
WORKDIR /workspace/SynthOrbisUNI

CMD ["/bin/bash"]
