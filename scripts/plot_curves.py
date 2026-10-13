#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真值/观测曲线 + 三方法预测曲线，画在一张 PNG（上下两栏）。
   上栏：真值(黑) + 带噪观测(灰) + EKF/LM/Ceres 预测(虚线)
         * 真值、观测按各自“原生时刻”画；预测画在“被预测的目标时刻 t_target=t_i+H”
   下栏：预测误差 = pred(t_target) − truth(t_target)
用法:
  python3 scripts/plot_curves.py --data <序列csv> --series gauss --dump <dump.csv> --out x.png \
      [--t0 6 --t1 14] [--title "..."] [--mark-csv <序列csv>]
"""
import argparse, csv, math, os
from PIL import Image, ImageDraw, ImageFont

ap = argparse.ArgumentParser()
ap.add_argument("--dump", required=True, help="predict_eval --dump-pred 输出")
ap.add_argument("--data", required=True, help="原始序列CSV(含真值/带噪观测)")
ap.add_argument("--series", default="gauss", choices=["gauss", "uniform", "combined"])
ap.add_argument("--out", required=True)
ap.add_argument("--t0", type=float, default=None); ap.add_argument("--t1", type=float, default=None)
ap.add_argument("--title", default="")
ap.add_argument("--mark-csv", default=None, help="检测大符模型重随机并画橙色竖线")
a = ap.parse_args()

def fnum(v):
    try:
        x = float(v); return None if (math.isnan(x) or math.isinf(x)) else x
    except Exception: return None

# 预测（在目标时刻）
D = list(csv.DictReader(open(a.dump)))
tt = [float(r["t_target"]) for r in D]
pred = {k: [fnum(r[k]) for r in D] for k in ("pred_ekf_deg", "pred_lm_deg", "pred_ceres_deg")}
truth_t = [fnum(r["truth_deg"]) for r in D]
# 真值 / 观测（按原生时刻）
C = list(csv.DictReader(open(a.data)))
dt = [float(r["t_wall_s"]) for r in C]
dtruth = [float(r["angle_truth_unwrap_rad"]) * 180.0 / math.pi for r in C]
dobs = [float(r["angle_obs_%s_unwrap_rad" % a.series]) * 180.0 / math.pi for r in C]
# 一致性检查：dump 的 truth_deg 应等于 data 在 t_target 处的真值
def interp(xs, ys, x):
    for i in range(len(xs) - 1):
        if xs[i] <= x <= xs[i + 1]:
            u = (x - xs[i]) / max(1e-12, xs[i + 1] - xs[i]); return ys[i] + (ys[i + 1] - ys[i]) * u
    return None
chk = [abs(truth_t[i] - (interp(dt, dtruth, tt[i]) or truth_t[i])) for i in range(0, len(tt), 50)]
print("一致性检查(真值|偏差 max=%.3f deg)" % max(chk))

lo = a.t0 if a.t0 is not None else dt[0]
hi = a.t1 if a.t1 is not None else dt[-1]
def inx(xs): return [i for i in range(len(xs)) if lo <= xs[i] <= hi]
idx_d = inx(dt); idx_p = inx(tt)

marks = []
if a.mark_csv:
    CC = list(csv.DictReader(open(a.mark_csv)))
    qt = [float(r["t_wall_s"]) for r in CC]; qy = [float(r["angle_truth_unwrap_rad"]) for r in CC]
    sp = [(qy[i+1]-qy[i])/(qt[i+1]-qt[i]) for i in range(len(qt)-1)]
    for i in range(1, len(sp)):
        if abs(sp[i]-sp[i-1]) > 0.4 and (not marks or qt[i]-marks[-1] > 1.0): marks.append(qt[i])

STY = [("pred_ekf_deg", (214, 39, 40), "EKF 预测"), ("pred_lm_deg", (31, 119, 180), "LM 预测"), ("pred_ceres_deg", (44, 160, 44), "Ceres 预测")]
W, H = 1240, 680; ML, MR = 82, 210; T0, T1, B0, B1 = 55, 400, 470, 620
def sx(v): return int(ML + (v - lo) / max(1e-9, hi - lo) * (W - ML - MR))
def mk_sy(y0v, y1v, p0, p1): return lambda v: int(p1 - (v - y0v) / max(1e-9, y1v - y0v) * (p1 - p0))

vals = [dtruth[i] for i in idx_d] + [dobs[i] for i in idx_d] + [pred[k][i] for i in idx_p for k in pred if pred[k][i] is not None]
y0v, y1v = min(vals), max(vals); pad = (y1v - y0v) * 0.06 + 1e-9; y0v -= pad; y1v += pad
syT = mk_sy(y0v, y1v, T0, T1)
errs = {k: [None if (pred[k][i] is None or truth_t[i] is None) else pred[k][i] - truth_t[i] for i in range(len(tt))] for k in pred}
evals = [abs(v) for i in idx_p for k in errs for v in [errs[k][i]] if v is not None]
emax = max([1.0] + evals) * 1.15
syE = mk_sy(-emax, emax, B0, B1)

img = Image.new("RGB", (W, H), "white"); d = ImageDraw.Draw(img)
CJK = ["/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/truetype/arphic/uming.ttc"]
def font(sz):
    for fp in CJK:
        if os.path.exists(fp):
            try: return ImageFont.truetype(fp, sz)
            except Exception: pass
    try: return ImageFont.load_default(size=sz)
    except Exception: return ImageFont.load_default()
f16, f13, f12 = font(16), font(13), font(12)
if a.title: d.text(((W - MR) / 2, 16), a.title, fill="black", font=f16, anchor="ma")

def panel(p0, p1, ymn, ymx, sy, ylab):
    for k in range(6):
        v = ymn + (ymx - ymn) * k / 5; yy = sy(v)
        d.line([(ML, yy), (W - MR, yy)], fill=(238, 238, 238))
        d.text((ML - 8, yy), "%.1f" % v, fill=(90, 90, 90), font=f12, anchor="rm")
    d.line([(ML, p0), (ML, p1)], fill=(40, 40, 40)); d.line([(ML, p1), (W - MR, p1)], fill=(40, 40, 40))
    d.text((24, (p0 + p1) / 2), ylab, fill=(60, 60, 60), font=f13, anchor="mm")
panel(T0, T1, y0v, y1v, syT, "转角 (deg)"); panel(B0, B1, -emax, emax, syE, "误差 (deg)")
for k in range(7):
    v = lo + (hi - lo) * k / 6; d.text((sx(v), B1 + 8), "%.2f" % v, fill=(90, 90, 90), font=f12, anchor="ma")
d.text(((ML + W - MR) / 2, H - 26), "时间 (s)", fill=(60, 60, 60), font=f13, anchor="ma")
d.line([(ML, syE(0)), (W - MR, syE(0))], fill=(120, 120, 120))
for mk in marks:
    if lo <= mk <= hi:
        d.line([(sx(mk), T0), (sx(mk), T1)], fill=(255, 140, 0), width=1)
        d.line([(sx(mk), B0), (sx(mk), B1)], fill=(255, 140, 0), width=1)

def line(xs, ys, idxs, sy, color, width=2, dash=False, dot=0):
    pts = [(sx(xs[i]), sy(ys[i])) for i in idxs if ys[i] is not None]
    if len(pts) < 2: return
    if dash:
        for j in range(len(pts) - 1):
            (x1, y1), (x2, y2) = pts[j], pts[j + 1]; seg = math.hypot(x2 - x1, y2 - y1)
            if seg < 1: continue
            n = max(1, int(seg // 12))
            for s in range(n + 1):
                u0 = s / (n + 1); u1 = u0 + 0.5 / (n + 1)
                d.line([(x1 + (x2-x1)*u0, y1 + (y2-y1)*u0), (x1 + (x2-x1)*u1, y1 + (y2-y1)*u1)], fill=color, width=width)
    else:
        d.line(pts, fill=color, width=width, joint="curve")
    if dot:
        for x, y in pts: d.ellipse([x-dot, y-dot, x+dot, y+dot], fill=color)

# 上栏：真值、观测按原生时刻；预测按目标时刻
line(dt, dtruth, idx_d, syT, (0, 0, 0), 3)
line(dt, dobs, idx_d, syT, (185, 185, 185), 1, dot=1)
for k, col, _ in STY: line(tt, pred[k], idx_p, syT, col, 2, dash=True)
# 下栏：误差按目标时刻
for k, col, _ in STY: line(tt, errs[k], idx_p, syE, col, 2, dash=True)

lx, ly = W - MR + 12, T0 + 6
d.line([(lx, ly), (lx+28, ly)], fill=(0, 0, 0), width=3); d.text((lx+34, ly), "真值", fill="black", font=f13, anchor="lm")
d.line([(lx, ly+24), (lx+28, ly+24)], fill=(185, 185, 185), width=2); d.text((lx+34, ly+24), "观测(含噪)", fill="black", font=f13, anchor="lm")
for k, (key, col, lab) in enumerate(STY):
    yy = ly + 54 + k * 24
    for s in range(0, 28, 8): d.line([(lx+s, yy), (lx+min(s+4, 28), yy)], fill=col, width=2)
    d.text((lx+34, yy), lab, fill="black", font=f13, anchor="lm")
if marks:
    yy = ly + 54 + 3 * 24
    d.line([(lx, yy), (lx+28, yy)], fill=(255, 140, 0), width=2); d.text((lx+34, yy), "模型重随机", fill="black", font=f13, anchor="lm")
img.save(a.out); print("写出", a.out)
