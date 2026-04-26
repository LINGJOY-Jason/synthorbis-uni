# =============================================
# SynthOrbis UNI — Linux LoongArch 构建环境
# 基础镜像：Loongnix 23 / Ubuntu LoongArch
# 目标：龙芯 (loongarch64-linux-gnu)
# =============================================

# 龙芯官方目前推荐使用 Loongnix OS 或在 Ubuntu LoongArch 上交叉编译
# 这里基于 Ubuntu 22.04 交叉编译；如需原生编译请使用 Loongnix 基础镜像
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# ── LoongArch 交叉编译工具链 ───────────────────
# LoongArch GCC 工具链仓库（由第三方维护）
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    curl \
    wget \
    gnupg \
    software-properties-common \
    && curl -fsSL https://github.com/loongson/loongarch64-toolchain/releases/download/latest/gcc-13.2.0-loongarch64.tar.xz \
       -o /tmp/gcc-loongarch.tar.xz \
    && mkdir -p /opt/loongarch \
    && tar -xf /tmp/gcc-loongarch.tar.xz -C /opt/loongarch --strip-components=1 \
    && rm /tmp/gcc-loongarch.tar.xz

# ── GCC 13.2.0 LoongArch ───────────────────────
ENV CC=/opt/loongarch/bin/loongarch64-linux-gnu-gcc
ENV CXX=/opt/loongarch/bin/loongarch64-linux-gnu-g++
ENV AR=/opt/loongarch/bin/loongarch64-linux-gnu-ar
ENV RANLIB=/opt/loongarch/bin/loongarch64-linux-gnu-ranlib
ENV SYSROOT=/opt/loongarch/sysroot
ENV RIME_TARGET_ARCH=loongarch64
ENV PATH=/opt/loongarch/bin:$PATH

WORKDIR /workspace/SynthOrbisUNI

CMD ["/bin/bash"]
