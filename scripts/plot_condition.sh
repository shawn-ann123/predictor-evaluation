#!/usr/bin/env bash
# 画任意条件下的"真值 vs 预测"曲线图(PNG)。
# 用法: scripts/plot_condition.sh <csv> <small|large> <window_ms> <horizon_ms> <gauss|uniform|combined> <t0> <t1> <out.png> [title]
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="$1"; MODE="$2"; WMS="$3"; HMS="$4"; SER="$5"; T0="$6"; T1="$7"; OUT="$8"; TITLE="${9:-}"
DUMP="$(mktemp /tmp/rune_dump.XXXXXX.csv)"
"$ROOT/build/predict_eval" --csv "$CSV" --mode "$MODE" --dump-pred "$DUMP" \
  --dump-window-ms "$WMS" --dump-horizon-ms "$HMS" --series "$SER" >/dev/null
python3 "$ROOT/scripts/plot_curves.py" --dump "$DUMP" --data "$CSV" --series "$SER" --out "$OUT" --t0 "$T0" --t1 "$T1" --title "$TITLE"
rm -f "$DUMP"
