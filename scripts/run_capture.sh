#!/usr/bin/env bash
# 一键采集：等周期 5ms 序列 + 逐帧序列（小符/大符各一组）。
# 需要模拟器正在运行，且本机可访问其场景控制端口（否则会用不了自动切模式）。
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-$ROOT/build/capture_rune_angles}"
IPC_DIR="${TALOS_IPC_DIR:-${XDG_RUNTIME_DIR:-/tmp}/daedalus-learning-$(id -u)}"
OUT="${OUT:-$ROOT/data}"
SAMPLES="${SAMPLES:-1000}"
PERIOD_MS="${PERIOD_MS:-5}"

[[ -x "$BIN" ]] || { echo "先运行 scripts/build.sh 构建 $BIN" >&2; exit 1; }

echo "==> 等周期 ${PERIOD_MS}ms 序列 (${SAMPLES} 点) -> $OUT/wallclock_${PERIOD_MS}ms"
"$BIN" --ipc-dir "$IPC_DIR" --out "$OUT/wallclock_${PERIOD_MS}ms" \
       --samples "$SAMPLES" --period-ms "$PERIOD_MS" --with-combined

echo "==> 逐帧(事件驱动)序列 (${SAMPLES} 帧) -> $OUT/per_frame"
"$BIN" --ipc-dir "$IPC_DIR" --out "$OUT/per_frame" \
       --samples "$SAMPLES" --on-new-frame --with-combined

echo "完成。"
