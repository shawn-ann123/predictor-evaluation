#!/usr/bin/env bash
# 构建 capture_rune_angles
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_ROOT="${DAEDALUS_SDK_ROOT:-$ROOT/../../完整版/sdk}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DDAEDALUS_SDK_ROOT="$SDK_ROOT"
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo "构建完成: $BUILD_DIR/{capture_rune_angles,predict_eval}"
