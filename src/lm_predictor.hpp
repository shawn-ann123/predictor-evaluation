// lm_predictor.hpp —— LM + 迭代加权最小二乘(IRLS) 方法
//   移植自 RP-26Rune/.../LM_IRLS_BigRunePhaseMotionFilter.cpp
//   配置默认值取自 power_rune.jsonc 的 big_phase_motion_estimate / small_phase_estimate。
//
//   大符：phase = A*cos(w t) + B*sin(w t) + b*t + C
//         外层 LM 迭代 w（w∈[1.884,2.000]），内层 IRLS 拟合 (A,B,b,C)
//         （时间权重 + Cauchy 距离权重 + ridge）
//   小符：phase = k*t + c（线性最小二乘；规则给小符恒定角速度 1.0472 rad/s）
#pragma once
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include "rune_models.hpp"

namespace rune {

struct SmallModel { double k = 0.0, c = 0.0, tref = 0.0; bool valid = false; };
struct BigModel { double A = 0, B = 0, b = 0, C = 0, w = 0, tref = 0; bool valid = false; };

// 大符预测：phase(t) = A cos(w(t-tref)) + B sin(w(t-tref)) + b(t-tref) + C
inline double predictBig(const BigModel& m, double t) {
  const double dt = t - m.tref;
  return m.A * std::cos(m.w * dt) + m.B * std::sin(m.w * dt) + m.b * dt + m.C;
}
inline double predictSmall(const SmallModel& m, double t) { return m.k * (t - m.tref) + m.c; }

struct LmConfig {
  int inner_max_iterations = 40;      // power_rune.jsonc
  int outer_max_iterations = 20;
  double omega_lower = 1.884, omega_upper = 2.000, omega_init = 1.942;
  double robust_scale = 2.5;          // Cauchy 尺度系数
  double inner_tolerance = 1e-2;
  double lambda_init = 1e-2, lambda_min = 1e-6, lambda_max = 1e6;
  double outer_tolerance = 1e-5;
  double max_omega_step = 0.05;
  double diff_eps_min = 1e-4, diff_eps_rel = 1e-3;
  double time_weight_high = 1.0, time_weight_low = 0.2;
  double ridge = 1e-9;
};

// 小符：φ = k·(t−tref) + c
// 用 LM 模块的迭代加权最小二乘(IRLS)：时间权重(旧数据权重低) + Cauchy 距离权重。
// 与 Ceres(纯最小二乘) 相比多一步鲁棒重加权。
inline SmallModel lmFitSmall(const std::vector<double>& t_in, const std::vector<double>& y_in,
                             const LmConfig& cfg = LmConfig{}) {
  SmallModel m;
  const std::size_t n = t_in.size();
  if (n < 2) return m;
  std::vector<double> sorted(t_in.begin(), t_in.end());
  std::sort(sorted.begin(), sorted.end());
  const double tref = sorted[n / 2];
  const double span = std::max(1e-9, sorted.back() - sorted.front());
  std::vector<double> x(n), yv(n), tw(n);
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = t_in[i] - tref;
    yv[i] = y_in[i];
    const double frac = (t_in[i] - sorted.front()) / span;
    tw[i] = cfg.time_weight_low + frac * (cfg.time_weight_high - cfg.time_weight_low);
  }
  std::vector<double> rw(n, 1.0);
  double k = 0.0, c = 0.0;
  for (int it = 0; it < cfg.inner_max_iterations; ++it) {
    double Sw = 0, Swx = 0, Swy = 0, Swxx = 0, Swxy = 0;
    for (std::size_t i = 0; i < n; ++i) {
      const double w = tw[i] * rw[i];
      Sw += w; Swx += w * x[i]; Swy += w * yv[i];
      Swxx += w * x[i] * x[i]; Swxy += w * x[i] * yv[i];
    }
    const double det = Swxx * Sw - Swx * Swx;
    if (std::fabs(det) < 1e-12) return m;
    const double k_new = (Sw * Swxy - Swx * Swy) / det;
    const double c_new = (Swxx * Swy - Swx * Swxy) / det;
    // 距离权重(Bauchy)，尺度随残差 RMS 自适应
    double sse_t = 0, sum_tw = 0;
    for (std::size_t i = 0; i < n; ++i) { const double r = yv[i] - (k_new * x[i] + c_new); sse_t += tw[i] * r * r; sum_tw += tw[i]; }
    const double rms = std::sqrt(std::max(1e-12, sse_t / std::max(1.0, sum_tw)));
    const double scale = std::max(1e-6, cfg.robust_scale * rms);
    for (std::size_t i = 0; i < n; ++i) {
      const double r = (yv[i] - (k_new * x[i] + c_new)) / scale;
      rw[i] = 1.0 / (1.0 + r * r);
    }
    const bool conv = (std::fabs(k_new - k) < 1e-9) && (std::fabs(c_new - c) < 1e-9);
    k = k_new; c = c_new;
    if (conv && it > 0) break;
  }
  m.k = k; m.c = c; m.tref = tref; m.valid = true;
  return m;
}

inline BigModel lmFitBig(const std::vector<double>& t_in, const std::vector<double>& y_in,
                         const LmConfig& cfg = LmConfig{}) {
  BigModel out;
  const std::size_t n = t_in.size();
  if (n < 6) return out;

  std::vector<double> sorted(t_in.begin(), t_in.end());
  std::sort(sorted.begin(), sorted.end());
  const double tref = sorted[n / 2];
  std::vector<double> t(n), y(n);
  for (std::size_t i = 0; i < n; ++i) { t[i] = t_in[i] - tref; y[i] = y_in[i]; }

  Eigen::VectorXd tw(n);
  const double span = std::max(1e-9, sorted.back() - sorted.front());
  for (std::size_t i = 0; i < n; ++i) {
    const double frac = (t_in[i] - sorted.front()) / span;
    tw[i] = cfg.time_weight_low + frac * (cfg.time_weight_high - cfg.time_weight_low);
  }
  const Eigen::VectorXd yv = Eigen::Map<const Eigen::VectorXd>(y.data(), n);

  auto fitLinear = [&](double w, Eigen::Vector4d& theta, double& cost) -> bool {
    Eigen::MatrixXd X(n, 4);
    for (std::size_t i = 0; i < n; ++i) {
      const double wt = w * t[i];
      X(i, 0) = std::cos(wt); X(i, 1) = std::sin(wt); X(i, 2) = t[i]; X(i, 3) = 1.0;
    }
    Eigen::VectorXd rw = Eigen::VectorXd::Ones(n);
    Eigen::Vector4d th = Eigen::Vector4d::Zero(); double c = 1e300; bool first = true;
    for (int it = 0; it < cfg.inner_max_iterations; ++it) {
      Eigen::VectorXd W = tw.cwiseProduct(rw);
      Eigen::Matrix4d H = X.transpose() * W.asDiagonal() * X + cfg.ridge * Eigen::Matrix4d::Identity();
      Eigen::Vector4d g = X.transpose() * W.asDiagonal() * yv;
      if (!H.allFinite() || !g.allFinite()) return false;
      Eigen::LDLT<Eigen::Matrix4d> ldlt(H);
      if (ldlt.info() != Eigen::Success) return false;
      Eigen::Vector4d th_new = ldlt.solve(g);
      if (!th_new.allFinite()) return false;
      const Eigen::VectorXd resid = yv - X * th_new;
      c = (W.array() * resid.array().square()).sum();
      if (!std::isfinite(c)) return false;
      const double rms = std::sqrt(std::max(1e-12, (tw.array() * resid.array().square()).sum() / n));
      const double scale = std::max(1e-6, cfg.robust_scale * rms);
      for (std::size_t i = 0; i < n; ++i) { const double r = resid(i) / scale; rw[i] = 1.0 / (1.0 + r * r); }
      if (first) { th = th_new; first = false; continue; }
      if ((th - th_new).norm() < cfg.inner_tolerance) { th = th_new; break; }
      th = th_new;
    }
    theta = th; cost = c; return true;
  };

  double w = std::clamp(cfg.omega_init, cfg.omega_lower, cfg.omega_upper);
  Eigen::Vector4d theta; double cost = 0;
  if (!fitLinear(w, theta, cost)) return out;
  Eigen::Vector4d best_theta = theta; double best_cost = cost, best_w = w;
  double lambda = std::max(cfg.lambda_min, cfg.lambda_init);
  for (int it = 0; it < cfg.outer_max_iterations; ++it) {
    const double eps = std::max(cfg.diff_eps_min, cfg.diff_eps_rel * std::fabs(w));
    const double wp = std::clamp(w + eps, cfg.omega_lower, cfg.omega_upper);
    const double wm = std::clamp(w - eps, cfg.omega_lower, cfg.omega_upper);
    if (wp == wm) break;
    Eigen::Vector4d tp, tm; double cp = 0, cm = 0;
    if (!fitLinear(wp, tp, cp) || !fitLinear(wm, tm, cm)) { lambda = std::min(cfg.lambda_max, lambda * 2); continue; }
    const double grad = (cp - cm) / (wp - wm);
    const double half = 0.5 * (wp - wm);
    double hess = (cp - 2 * best_cost + cm) / (half * half);
    if (!std::isfinite(grad) || !std::isfinite(hess)) break;
    if (hess < 0) hess = -hess;
    double step = std::clamp(-grad / (hess + lambda), -cfg.max_omega_step, cfg.max_omega_step);
    const double wc = std::clamp(w + step, cfg.omega_lower, cfg.omega_upper);
    if (std::fabs(wc - w) < cfg.outer_tolerance) break;
    Eigen::Vector4d tc; double cc = 0;
    if (fitLinear(wc, tc, cc) && cc < best_cost) {
      best_w = wc; best_theta = tc; best_cost = cc; w = wc;
      lambda = std::max(cfg.lambda_min, lambda * 0.5);
    } else {
      lambda = std::min(cfg.lambda_max, lambda * 2.0);
    }
  }
  out.A = best_theta(0); out.B = best_theta(1); out.b = best_theta(2); out.C = best_theta(3);
  out.w = best_w; out.tref = tref; out.valid = true;
  return out;
}

}  // namespace rune
