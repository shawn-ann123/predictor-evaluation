#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
滑动窗口角度预测评估示例（下游用法演示，非最终评估框架）。

读取 data/*/rune_<mode>_sequences.csv，对每个"带观测误差"的序列做滑动窗口预测：
    用窗口内过去 W 个 (时间, 角度) 观测，预测 H 个采样点之后的真值角度；
    多方法对比：persistence(保持) / linear(线性外推) / quadratic(二次外推)。
以 angle_truth_unwrap_rad 作为评测基准真值，误差统一换算成度。

用法:
    python3 scripts/eval_example.py --csv data/per_frame/rune_small_sequences.csv \
        --window 8 --horizon 5
    python3 scripts/eval_example.py --data data/per_frame --window 8 --horizon 5   # 遍历目录
"""
import argparse
import csv
import math
import os
import sys

R2D = 180.0 / math.pi


def read_csv(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    cols = {k: [float(r[k]) for r in rows] for k in rows[0]}
    return cols


def solve(a, b):
    """高斯消元解 a x = b（就地拷贝）。"""
    n = len(a)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        if abs(m[p][c]) < 1e-12:
            return None
        m[c], m[p] = m[p], m[c]
        pv = m[c][c]
        for j in range(c, n + 1):
            m[c][j] /= pv
        for r in range(n):
            if r != c:
                f = m[r][c]
                for j in range(c, n + 1):
                    m[r][j] -= f * m[c][j]
    return [m[i][n] for i in range(n)]


def polyfit_predict(ts, ys, t_target, degree):
    """最小二乘多项式拟合后在 t_target 处求值（等价 np.polyfit+polyval）。"""
    n = degree + 1
    a = [[sum(t ** (i + j) for t in ts) for j in range(n)] for i in range(n)]
    b = [sum((t ** i) * y for t, y in zip(ts, ys)) for i in range(n)]
    c = solve(a, b)
    if c is None:
        return ys[-1]
    return sum(ci * (t_target ** i) for i, ci in enumerate(c))


def predict(method, ts, ys, t_target):
    if method == "persistence":
        return ys[-1]
    if method == "linear":
        return polyfit_predict(ts, ys, t_target, 1)
    if method == "quadratic":
        return polyfit_predict(ts, ys, t_target, 2)
    raise ValueError(method)


def evaluate(cols, obs_key, window, horizon, methods):
    ts = cols["t_wall_s"]
    ys = cols[obs_key]                # 观测（已展开连续角度，弧度）
    truth = cols["angle_truth_unwrap_rad"]
    n = len(ts)
    per = {m: [] for m in methods}
    for i in range(window - 1, n - horizon):
        tgt = i + horizon
        wts = ts[i - window + 1:i + 1]
        wys = ys[i - window + 1:i + 1]
        for m in methods:
            pred = predict(m, wts, wys, ts[tgt])
            per[m].append((pred - truth[tgt]) * R2D)
    return per


def stats(errs):
    n = len(errs)
    if n == 0:
        return dict(n=0, rmse=float("nan"), mae=float("nan"), maxe=float("nan"))
    rmse = math.sqrt(sum(e * e for e in errs) / n)
    mae = sum(abs(e) for e in errs) / n
    return dict(n=n, rmse=rmse, mae=mae, maxe=max(abs(e) for e in errs))


def run_file(path, window, horizon, methods):
    cols = read_csv(path)
    print(f"\n== {path}  (n={len(cols['t_wall_s'])}, window={window}, horizon={horizon} pts) ==")
    obs_series = [k for k in cols if k.startswith("angle_obs_") and k.endswith("_unwrap_rad")]
    hdr = f"{'obs_series':<28}" + "".join(f"{m:>18}" for m in methods)
    print(hdr)
    print(f"{'':<28}" + "".join(f"{'RMSE°/MAE°':>18}" for _ in methods))
    for obs in obs_series:
        res = evaluate(cols, obs, window, horizon, methods)
        line = f"{obs:<28}"
        for m in methods:
            s = stats(res[m])
            line += f"{s['rmse']:>9.3f}/{s['mae']:<8.3f}"
        print(line)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv")
    ap.add_argument("--data", help="目录: 处理其中的 rune_*_sequences.csv")
    ap.add_argument("--window", type=int, default=8)
    ap.add_argument("--horizon", type=int, default=5)
    ap.add_argument("--methods", default="persistence,linear,quadratic")
    args = ap.parse_args()
    methods = [m.strip() for m in args.methods.split(",") if m.strip()]

    files = []
    if args.csv:
        files.append(args.csv)
    if args.data:
        for m in ("small", "large"):
            p = os.path.join(args.data, f"rune_{m}_sequences.csv")
            if os.path.exists(p):
                files.append(p)
    if not files:
        ap.error("需要 --csv 或 --data")

    for p in files:
        run_file(p, args.window, args.horizon, methods)


if __name__ == "__main__":
    main()
