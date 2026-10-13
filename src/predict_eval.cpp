// predict_eval.cpp —— 三种方法(EKF / LM / Ceres) 滑动窗口预测评估
//   读取 data/**/rune_<mode>_sequences.csv，对每条观测序列(高斯/均匀/合成)预测未来角度。
//   * EKF：按原始实现"连续递归"运行（状态不重置），在每一步预测 H 毫秒后的角度。
//          （递归滤波没有窗口概念，输出 window 列记为 0）
//   * LM / Ceres：对每个窗口（最近 W 个观测）重新拟合模型后再外推。
//   预测目标时刻取"第一个不早于 t_i+H 的真实样本时刻"，与真值(*_unwrap)比较。
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ceres_predictor.hpp"
#include "ekf_predictor.hpp"
#include "lm_predictor.hpp"
#include "rune_models.hpp"

namespace {
std::vector<std::string> splitCsv(const std::string& line) {
  std::vector<std::string> out; std::string cur;
  for (char c : line) { if (c == ',') { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
  out.push_back(cur); return out;
}
struct Column { std::vector<double> t, truth; std::map<std::string, std::vector<double>> obs; };
bool loadCsv(const std::string& path, Column* col, std::string* err) {
  std::ifstream f(path);
  if (!f) { *err = "无法打开 " + path; return false; }
  std::string line;
  if (!std::getline(f, line)) { *err = "空文件"; return false; }
  std::vector<std::string> header = splitCsv(line);
  std::map<std::string, int> idx;
  for (std::size_t i = 0; i < header.size(); ++i) idx[header[i]] = static_cast<int>(i);
  auto need = [&](const std::string& k) { auto it = idx.find(k); return it == idx.end() ? -1 : it->second; };
  const int it_t = need("t_wall_s"), it_tr = need("angle_truth_unwrap_rad");
  if (it_t < 0 || it_tr < 0) { *err = "缺列 t_wall_s/angle_truth_unwrap_rad"; return false; }
  const char* names[] = {"angle_obs_gauss_unwrap_rad", "angle_obs_uniform_unwrap_rad", "angle_obs_combined_unwrap_rad"};
  std::vector<std::pair<std::string, int>> oi;
  for (const char* n : names) { const int k = need(n); if (k >= 0) oi.emplace_back(n, k); }
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    const std::vector<std::string> v = splitCsv(line);
    col->t.push_back(std::atof(v[it_t].c_str()));
    col->truth.push_back(std::atof(v[it_tr].c_str()));
    for (auto& [name, k] : oi) col->obs[name].push_back(std::atof(v[k].c_str()));
  }
  return true;
}
struct Acc { double sse = 0, sae = 0, mx = 0; long n = 0;
  void add(double e) { sse += e * e; sae += std::fabs(e); mx = std::max(mx, std::fabs(e)); ++n; }
  double rmse() const { return n ? std::sqrt(sse / n) : NAN; }
  double mae() const { return n ? sae / n : NAN; } };
}  // namespace

int main(int argc, char** argv) {
  std::string csv, mode, out = "results/metrics.csv";
  std::vector<double> window_ms{200, 500, 1000, 2000};  // 拟合窗口=固定“时间跨度”
  rune::BigEkfConfig big_cfg;      // 大符 EKF 可调参数
  double warmup_ms = 0.0;          // 统计时跳过序列开头这么多毫秒(冷启动/暖机)
  std::string dump_pred;           // 若设置：导出逐锚点的 真值/观测/三方法预测
  double dump_w = 1000.0, dump_h = 100.0;
  std::string dump_series = "gauss";
  std::vector<double> horizons{20, 40, 60, 100, 150, 200};
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto val = [&]() { return std::string(argv[++i]); };
    if (a == "--csv") csv = val();
    else if (a == "--mode") mode = val();
    else if (a == "--out") out = val();
    else if (a == "--window-ms") { window_ms.clear(); std::stringstream ss(val()); std::string s; while (std::getline(ss, s, ',')) window_ms.push_back(std::atof(s.c_str())); }
    else if (a == "--horizons-ms") { horizons.clear(); std::stringstream ss(val()); std::string s; while (std::getline(ss, s, ',')) horizons.push_back(std::atof(s.c_str())); }
    else if (a == "--warmup-ms") warmup_ms = std::atof(val().c_str());
    else if (a == "--big-r") big_cfg.r = std::atof(val().c_str());
    else if (a == "--big-p0") { std::stringstream ss(val()); std::string s; for (int k = 0; k < 5 && std::getline(ss, s, ','); ++k) big_cfg.p0[k] = std::atof(s.c_str()); }
    else if (a == "--big-q") { std::stringstream ss(val()); std::string s; for (int k = 0; k < 5 && std::getline(ss, s, ','); ++k) big_cfg.q[k] = std::atof(s.c_str()); }
    else if (a == "--dump-pred") dump_pred = val();
    else if (a == "--dump-window-ms") dump_w = std::atof(val().c_str());
    else if (a == "--dump-horizon-ms") dump_h = std::atof(val().c_str());
    else if (a == "--series") dump_series = val();
    else { std::fprintf(stderr, "未知参数 %s\n", a.c_str()); return 2; }
  }
  if (csv.empty() || (mode != "small" && mode != "large")) { std::fprintf(stderr, "需要 --csv 与 --mode small|large\n"); return 2; }
  const bool big = (mode == "large");
  Column col; std::string err;
  if (!loadCsv(csv, &col, &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
  const int N = static_cast<int>(col.t.size());
  std::printf("载入 %s: N=%d, 观测序列=%zu\n", csv.c_str(), N, col.obs.size());
  if (big) std::printf("  BigEKF: R=%.4g  P0=[%.3g %.3g %.3g %.3g %.3g]  Q=[%.3g %.3g %.3g %.3g %.3g]\n", big_cfg.r, big_cfg.p0[0],big_cfg.p0[1],big_cfg.p0[2],big_cfg.p0[3],big_cfg.p0[4], big_cfg.q[0],big_cfg.q[1],big_cfg.q[2],big_cfg.q[3],big_cfg.q[4]);
  const double deg = rune::kDeg;

  // ---- 导出逐锚点曲线（真值 / 观测 / EKF / LM / Ceres），供画图 ----
  if (!dump_pred.empty()) {
    const std::string key = "angle_obs_" + dump_series + "_unwrap_rad";
    auto ito = col.obs.find(key);
    if (ito == col.obs.end()) { std::fprintf(stderr, "未知 series: %s\n", dump_series.c_str()); return 1; }
    const std::vector<double>& y = ito->second;
    std::vector<rune::SmallEKF> se; std::vector<rune::BigEKF> be;
    if (big) { be.reserve(N); rune::BigEKF e(big_cfg); for (int k = 0; k < N; ++k) { e.update(y[k], col.t[k]); be.push_back(e); } }
    else { se.reserve(N); rune::SmallEKF e; for (int k = 0; k < N; ++k) { e.update(y[k], col.t[k]); se.push_back(e); } }
    std::ofstream df(dump_pred);
    df << "i,t_i,obs_deg,t_target,truth_deg,pred_ekf_deg,pred_lm_deg,pred_ceres_deg\n";
    for (int i = 0; i < N - 1; ++i) {
      const double t_want = col.t[i] + dump_h / 1000.0;
      auto it = std::lower_bound(col.t.begin() + i + 1, col.t.end(), t_want);
      if (it == col.t.end()) break;
      const int j = static_cast<int>(it - col.t.begin());
      const double tp = col.t[j];
      const int s = static_cast<int>(std::lower_bound(col.t.begin(), col.t.begin() + i + 1, col.t[i] - dump_w / 1000.0) - col.t.begin());
      double pe = big ? (be[i].angle() + be[i].predictChange(tp - col.t[i])) : (se[i].angle() + se[i].predictChange(tp - col.t[i]));
      double pl = NAN, pc = NAN;
      if (i - s + 1 >= 4) {
        std::vector<double> wt(col.t.begin() + s, col.t.begin() + i + 1);
        std::vector<double> wy(y.begin() + s, y.begin() + i + 1);
        if (big) {
          auto m1 = rune::lmFitBig(wt, wy); if (m1.valid) pl = rune::predictBig(m1, tp);
          auto m2 = rune::ceresFitBig(wt, wy); if (m2.valid) pc = rune::predictBig(m2, tp);
        } else {
          auto m1 = rune::lmFitSmall(wt, wy); if (m1.valid) pl = rune::predictSmall(m1, tp);
          auto m2 = rune::ceresFitSmall(wt, wy); if (m2.valid) pc = rune::predictSmall(m2, tp);
        }
      }
      df << i << ',' << col.t[i] << ',' << y[i]*deg << ',' << tp << ',' << col.truth[j]*deg
         << ',' << pe*deg << ',' << (std::isnan(pl)?NAN:pl*deg) << ',' << (std::isnan(pc)?NAN:pc*deg) << "\n";
    }
    std::printf("导出逐锚点曲线 -> %s (window=%.0fms, horizon=%.0fms, series=%s)\n", dump_pred.c_str(), dump_w, dump_h, dump_series.c_str());
    return 0;
  }

  std::ofstream of(out);
  of << "csv,mode,series,method,window,horizon_ms,n,rmse_deg,mae_deg,max_deg\n";
  auto emit = [&](const std::string& series, const char* name, const Acc& a, int w, double hms) {
    of << csv << ',' << mode << ',' << series << ',' << name << ',' << w << ',' << hms << ','
       << a.n << ',' << a.rmse() << ',' << a.mae() << ',' << a.mx << "\n";
  };

  for (auto& [obs_name, y] : col.obs) {
    // ---- EKF：连续递归 ----
    std::vector<rune::SmallEKF> se; std::vector<rune::BigEKF> be;
    if (big) { be.reserve(N); rune::BigEKF e(big_cfg); for (int k = 0; k < N; ++k) { e.update(y[k], col.t[k]); be.push_back(e); } }
    else { se.reserve(N); rune::SmallEKF e; for (int k = 0; k < N; ++k) { e.update(y[k], col.t[k]); se.push_back(e); } }
    for (double hms : horizons) {
      Acc acc;
      for (int i = 0; i < N - 1; ++i) {
        if (col.t[i] < col.t[0] + warmup_ms / 1000.0) continue;
        const double t_want = col.t[i] + hms / 1000.0;
        auto it = std::lower_bound(col.t.begin() + i + 1, col.t.end(), t_want);
        if (it == col.t.end()) break;
        const int j = static_cast<int>(it - col.t.begin());
        const double dt = col.t[j] - col.t[i];
        const double p = big ? (be[i].angle() + be[i].predictChange(dt)) : (se[i].angle() + se[i].predictChange(dt));
        acc.add((p - col.truth[j]) * deg);
      }
      emit(obs_name, "ekf", acc, 0, hms);
    }
    // ---- LM / Ceres：滑动窗口拟合（窗口=最近 wms 毫秒的观测） ----
    for (double wms : window_ms) {
      for (double hms : horizons) {
        Acc lm, ce; long used = 0;
        for (int i = 0; i < N - 1; ++i) {
          if (col.t[i] < col.t[0] + warmup_ms / 1000.0) continue;
          const double t_want = col.t[i] + hms / 1000.0;
          auto it = std::lower_bound(col.t.begin() + i + 1, col.t.end(), t_want);
          if (it == col.t.end()) break;
          const int j = static_cast<int>(it - col.t.begin());
          const double tp = col.t[j];
          const int s = static_cast<int>(std::lower_bound(col.t.begin(), col.t.begin() + i + 1, col.t[i] - wms / 1000.0) - col.t.begin());
          if (i - s + 1 < 4) continue;  // 太少样本不拟合
          std::vector<double> wt(col.t.begin() + s, col.t.begin() + i + 1);
          std::vector<double> wy(y.begin() + s, y.begin() + i + 1);
          if (big) {
            auto m1 = rune::lmFitBig(wt, wy); if (m1.valid) lm.add((rune::predictBig(m1, tp) - col.truth[j]) * deg);
            auto m2 = rune::ceresFitBig(wt, wy); if (m2.valid) ce.add((rune::predictBig(m2, tp) - col.truth[j]) * deg);
          } else {
            auto m1 = rune::lmFitSmall(wt, wy); if (m1.valid) lm.add((rune::predictSmall(m1, tp) - col.truth[j]) * deg);
            auto m2 = rune::ceresFitSmall(wt, wy); if (m2.valid) ce.add((rune::predictSmall(m2, tp) - col.truth[j]) * deg);
          }
          ++used;
        }
        emit(obs_name, "lm", lm, static_cast<int>(wms), hms); emit(obs_name, "ceres", ce, static_cast<int>(wms), hms);
      }
    }
    std::printf("  完成 series=%s\n", obs_name.c_str());
  }
  std::printf("写出 %s\n", out.c_str());
  return 0;
}
