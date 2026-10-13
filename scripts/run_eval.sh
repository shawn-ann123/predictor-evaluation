#!/usr/bin/env bash
# 对 4 个数据集(2 采样方式 x 2 模式)跑三方法滑动窗口评估，汇总到 results/metrics_all.csv
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/predict_eval"
OUT="$ROOT/results"
WINDOWS="${WINDOWS:-200,500,1000,2000}"
HORIZONS="${HORIZONS:-20,40,60,100,150,200}"
WARMUP="${WARMUP:-1000}"
mkdir -p "$OUT"
[[ -x "$BIN" ]] || { echo "先构建: scripts/build.sh" >&2; exit 1; }

ALL="$OUT/metrics_all.csv"; : > "$ALL"
emit() {  # <csv> <mode> <tag>
  local tmp="$OUT/_tmp.csv"
  "$BIN" --csv "$1" --mode "$2" --out "$tmp" --window-ms "$WINDOWS" --horizons-ms "$HORIZONS" --warmup-ms "$WARMUP" >/dev/null
  tail -n +2 "$tmp" >> "$ALL"
  echo "  done $3"
}
printf 'csv,mode,series,method,window,horizon_ms,n,rmse_deg,mae_deg,max_deg\n' > "$ALL"
echo "== wallclock_5ms =="
emit "$ROOT/data/wallclock_5ms/rune_small_sequences.csv" small "5ms/small"
emit "$ROOT/data/wallclock_5ms/rune_large_sequences.csv" large "5ms/large"
echo "== per_frame =="
emit "$ROOT/data/per_frame/rune_small_sequences.csv" small "perframe/small"
emit "$ROOT/data/per_frame/rune_large_sequences.csv" large "perframe/large"
echo "汇总 -> $ALL ($(wc -l < "$ALL") 行)"
