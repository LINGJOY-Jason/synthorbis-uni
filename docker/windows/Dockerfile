# =============================================
# SynthOrbis UNI — Windows (MinGW-w64) 交叉编译环境
# 从 Linux 交叉编译 Windows .exe / .dll
# =============================================

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

# ── MinGW-w64 交叉编译工具链 ───────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    mingw-w64 \
    wine \
    wget \
    curl \
    git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ── Windows SDK 头文件（可选，用于原生 Windows 开发）──
# 如需完整 Windows API 头文件，请在 Windows 主机直接编译
# 此容器主要用于 CI 的无头交叉编译验证

# ── 工具链路径 ─────────────────────────────────
ENV CC=x86_64-w64-mingw32-gcc
ENV CXX=x86_64-w64-mingw32-g++
ENV CMAKE_SYSTEM_NAME=Windows
ENV CMAKE_SYSTEM_PROCESSOR=x86_64

WORKDIR /workspace/SynthOrbisUNI

CMD ["/bin/bash"]
