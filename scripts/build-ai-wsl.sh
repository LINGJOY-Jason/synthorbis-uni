#!/bin/sh
# SynthOrbis AI — WSL Ubuntu 构建脚本（带 ONNX Runtime）

set -e

ORT_ROOT=/tmp/onnxruntime-linux-x64-1.21.0
SRC=/mnt/d/SynthOrbisUNI/engine/synthorbis-ai
BUILD=/tmp/synthorbis-ai-build-ort

echo "=== CMake Configure ==="
mkdir -p "$BUILD"
cd "$BUILD"

cmake "$SRC" \
  -DCMAKE_BUILD_TYPE=Release \
  -DONNXRUNTIME_INCLUDE_DIRS="$ORT_ROOT/include" \
  -DONNXRUNTIME_LIBRARIES="$ORT_ROOT/lib/libonnxruntime.so" \
  -DSYNTHORBIS_AI_BUILD_TESTS=ON

echo ""
echo "=== Build ==="
cmake --build . --parallel 4

echo ""
echo "=== Done ==="
echo "Artifacts in: $BUILD"
ls -la "$BUILD"/*.a 2>/dev/null || true
