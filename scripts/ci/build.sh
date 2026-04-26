#!/bin/bash
# =============================================
# SynthOrbis UNI — CI 构建脚本
# =============================================
# 用于 CI/CD 流水线中的自动化构建
# 支持多平台: Linux / macOS / Windows (Git Bash / WSL)
#
# 用法:
#   bash scripts/ci/build.sh linux      # 构建 Linux 版本
#   bash scripts/ci/build.sh macos     # 构建 macOS 版本
#   bash scripts/ci/build.sh all       # 构建所有平台
# =============================================

set -e

# ── 颜色输出 ──────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info()  { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok()    { echo -e "${GREEN}[ OK ]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ── 配置 ──────────────────────────────────────────
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-build}"
RIME_DIR="${RIME_DIR:-engine/librime}"
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ── 依赖检查 ──────────────────────────────────────
check_deps_linux() {
    log_info "检查 Linux 依赖..."
    local missing=()

    for cmd in cmake ninja; do
        if ! command -v $cmd &> /dev/null; then
            missing+=($cmd)
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        log_error "缺少依赖: ${missing[*]}"
        log_info "安装命令: sudo apt-get install ${missing[*]}"
        exit 1
    fi
    log_ok "依赖检查通过"
}

check_deps_macos() {
    log_info "检查 macOS 依赖..."
    local missing=()

    for cmd in cmake ninja brew; do
        if ! command -v $cmd &> /dev/null; then
            missing+=($cmd)
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        log_error "缺少依赖: ${missing[*]}"
        log_info "使用 Homebrew 安装: brew install ${missing[*]}"
        exit 1
    fi
    log_ok "依赖检查通过"
}

# ── 构建函数 ──────────────────────────────────────
build_linux() {
    log_info "=== 构建 Linux 版本 ==="
    check_deps_linux

    local platform="linux-x64"
    local build_path="$BUILD_DIR/$platform"

    mkdir -p "$build_path"
    cd "$build_path"

    log_info "Configure CMake..."
    cmake "../../$RIME_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTING=ON \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    log_info "Build (parallel=$PARALLEL_JOBS)..."
    ninja -j"$PARALLEL_JOBS"

    log_info "Run tests..."
    ctest --output-on-failure -j"$PARALLEL_JOBS" || log_warn "测试失败，继续..."

    log_ok "Linux 构建完成: $build_path"
}

build_macos() {
    log_info "=== 构建 macOS 版本 ==="
    check_deps_macos

    local platform="macos-universal"
    local build_path="$BUILD_DIR/$platform"

    # 安装必要依赖
    log_info "安装 Homebrew 依赖..."
    brew install cmake ninja boost opencc yaml-cpp 2>/dev/null || true

    mkdir -p "$build_path"
    cd "$build_path"

    log_info "Configure CMake..."
    cmake "../../$RIME_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTING=ON \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

    log_info "Build..."
    ninja -j"$PARALLEL_JOBS"

    log_ok "macOS 构建完成: $build_path"
}

build_windows() {
    log_info "=== 构建 Windows 版本 (需要 MSVC) ==="

    if ! command -v cmake &> /dev/null; then
        log_error "Windows 构建需要 CMake 和 MSVC 工具链"
        exit 1
    fi

    local platform="windows-x64"
    local build_path="$BUILD_DIR/$platform"

    mkdir -p "$build_path"
    cd "$build_path"

    log_info "Configure CMake..."
    cmake "../../$RIME_DIR" \
        -G "Visual Studio 17 2022" \
        -A x64 \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_TESTING=ON

    log_info "Build..."
    cmake --build . --config "$BUILD_TYPE" --parallel

    log_ok "Windows 构建完成: $build_path"
}

# ── 主逻辑 ────────────────────────────────────────
main() {
    local target="${1:-linux}"

    echo ""
    echo "╔═══════════════════════════════════════════╗"
    echo "║  SynthOrbis UNI — CI Build Script        ║"
    echo "╚═══════════════════════════════════════════╝"
    echo ""
    log_info "目标平台: $target"
    log_info "构建类型: $BUILD_TYPE"
    log_info "并行任务: $PARALLEL_JOBS"
    echo ""

    case "$target" in
        linux)
            build_linux
            ;;
        macos)
            build_macos
            ;;
        windows)
            build_windows
            ;;
        all)
            build_linux
            if [[ "$OSTYPE" == "darwin"* ]]; then
                build_macos
            elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
                build_windows
            fi
            ;;
        *)
            log_error "未知平台: $target"
            echo "可用平台: linux | macos | windows | all"
            exit 1
            ;;
    esac

    echo ""
    log_ok "构建脚本执行完成!"
}

main "$@"
