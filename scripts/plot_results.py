#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 results/metrics_all.csv 生成 RMSE-预测时域 折线图（纯 Python，输出 SVG）。"""
import csv, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = list(csv.DictReader(open(os.path.join(ROOT, "results", "metrics_all.csv"))))

def ds(r): return "5ms" if "wallclock" in r["csv"] else "per_frame"
def val(d, m, ser, method, win, h):
    for r in rows:
        if ds(r) == d and r["mode"] == m and r["series"] == ser and r["method"] == method \
           and r["window"] == win and r["horizon_ms"] == h:
            return float(r["rmse_deg"])
    return None

HORIZONS = ["20", "40", "60", "100", "150", "200"]
WIN = "1000"   # 拟合窗口 1000ms
SER = "angle_obs_gauss_unwrap_rad"
METHODS = [("ekf", "#d62728", "EKF"), ("lm", "#1f77b4", "LM+IRLS"), ("ceres", "#2ca02c", "Ceres")]
PANELS = [("5ms", "small"), ("5ms", "large"), ("per_frame", "small"), ("per_frame", "large")]

W, H = 1000, 720
PW, PH = 400, 280          # panel size
MX, MY = 70, 60            # margins inside panel
xs = [20, 40, 60, 100, 150, 200]

def sx(v, x0): return x0 + MX + (v - 20) / (200 - 20) * (PW - 2 * MX)
def sy(v, y0, ymax): return y0 + PH - MY - (v / ymax) * (PH - 2 * MY)

out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" font-family="sans-serif" font-size="13">',
       f'<rect width="{W}" height="{H}" fill="white"/>',
       '<text x="500" y="26" text-anchor="middle" font-size="18">能量机关转角预测 RMSE vs 预测时域（高斯观测, 拟合窗口=1000ms）</text>']
for pi, (d, m) in enumerate(PANELS):
    x0 = 30 + (pi % 2) * (PW + 20)
    y0 = 50 + (pi // 2) * (PH + 20)
    series = {meth: [val(d, m, SER, meth, "0" if meth == "ekf" else WIN, h) for h in HORIZONS] for meth, _, _ in METHODS}
    ymax = max(1e-6, max(v for s in series.values() for v in s if v is not None) * 1.1)
    out.append(f'<rect x="{x0}" y="{y0}" width="{PW}" height="{PH}" fill="#fafafa" stroke="#ccc"/>')
    out.append(f'<text x="{x0+PW/2}" y="{y0+24}" text-anchor="middle" font-size="15">{d} · {"小符" if m=="small" else "大符"}</text>')
    # axes
    ax0, ay1 = x0 + MX, y0 + PH - MY
    ax1, ay0 = x0 + PW - MX, y0 + MY
    out.append(f'<line x1="{ax0}" y1="{ay0}" x2="{ax0}" y2="{ay1}" stroke="#333"/>')
    out.append(f'<line x1="{ax0}" y1="{ay1}" x2="{ax1}" y2="{ay1}" stroke="#333"/>')
    for gv in range(0, int(ymax) + 1):
        yy = sy(gv, y0, ymax)
        if ay0 - 2 <= yy <= ay1 + 2:
            out.append(f'<line x1="{ax0}" y1="{yy}" x2="{ax1}" y2="{yy}" stroke="#eee"/>')
            out.append(f'<text x="{ax0-6}" y="{yy+4}" text-anchor="end" fill="#666">{gv}</text>')
    for v in xs:
        xx = sx(v, x0)
        out.append(f'<text x="{xx}" y="{ay1+18}" text-anchor="middle" fill="#666">{v}</text>')
    out.append(f'<text x="{x0+PW/2}" y="{y0+PH-8}" text-anchor="middle" fill="#444">预测时域 (ms)</text>')
    out.append(f'<text x="{x0+14}" y="{y0+PH/2}" text-anchor="middle" fill="#444" transform="rotate(-90 {x0+14} {y0+PH/2})">RMSE (deg)</text>')
    for meth, color, label in METHODS:
        pts = [(sx(xs[i], x0), sy(series[meth][i], y0, ymax)) for i in range(len(xs)) if series[meth][i] is not None]
        if not pts: continue
        out.append(f'<polyline fill="none" stroke="{color}" stroke-width="2" points="' +
                   " ".join(f"{px:.1f},{py:.1f}" for px, py in pts) + '"/>')
        for px, py in pts:
            out.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="2.5" fill="{color}"/>')
    # legend
    lx = ax0 + 8
    for k, (meth, color, label) in enumerate(METHODS):
        ly = ay0 + 14 + k * 16
        out.append(f'<line x1="{lx}" y1="{ly}" x2="{lx+18}" y2="{ly}" stroke="{color}" stroke-width="2"/>')
        out.append(f'<text x="{lx+24}" y="{ly+4}" fill="#333">{label}</text>')
out.append('</svg>')
# 2x2 布局需要更大画布
svg = "\n".join(out).replace(f'width="1000" height="720"', f'width="{2*PW+70}" height="{2*PH+90}"', 1)
path = os.path.join(ROOT, "results", "rmse_vs_horizon.svg")
open(path, "w").write(svg)
print("写出", path)
