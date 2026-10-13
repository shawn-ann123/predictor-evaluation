// ceres_predictor.hpp —— Ceres Solver 曲线拟合方法
//   用 Ceres 非线性最小二乘拟合与 LM 方法相同的运动方程：
//     大符：phase = A cos(w(t-tref)) + B sin(w(t-tref)) + b(t-tref) + C，
//           参数 [A,B,b,C,w]，其中 w 约束在 [1.884, 2.000]（规则区间）。
//     小符：phase = k(t-tref) + c（线性；规则给小符恒定角速度）。
//   与 LM 方法使用同一物理模型，差别在求解器（Ceres vs 自写 LM+IRLS）。
#pragma once
#include <ceres/ceres.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "lm_predictor.hpp"   // 复用 SmallModel/BigModel/predictBig/predictSmall
#include "rune_models.hpp"

namespace rune {

struct CeresBigResidual {
  CeresBigResidual(double t, double y, double tref) : t_(t), y_(y), tref_(tref) {}
  template <typename T>
  bool operator()(const T* const p, T* r) const {
    const T dt = T(t_) - T(tref_);
    r[0] = p[0] * cos(p[4] * dt) + p[1] * sin(p[4] * dt) + p[2] * dt + p[3] - T(y_);
    return true;
  }
  double t_, y_, tref_;
};

struct CeresSmallResidual {
  CeresSmallResidual(double t, double y, double tref) : t_(t), y_(y), tref_(tref) {}
  template <typename T>
  bool operator()(const T* const p, T* r) const {
    r[0] = p[0] * (T(t_) - T(tref_)) + p[1] - T(y_);
    return true;
  }
  double t_, y_, tref_;
};

inline void ceresSolve(ceres::Problem& problem, ceres::Solver::Options& opt) {
  opt.linear_solver_type = ceres::DENSE_QR;
  opt.minimizer_progress_to_stdout = false;
  opt.logging_type = ceres::SILENT;
  opt.max_num_iterations = 100;
  ceres::Solver::Summary summary;
  ceres::Solve(opt, &problem, &summary);
}

inline BigModel ceresFitBig(const std::vector<double>& t, const std::vector<double>& y,
                            const LmConfig& cfg = LmConfig{}) {
  BigModel m;
  const std::size_t n = t.size();
  if (n < 6) return m;
  std::vector<double> sorted(t.begin(), t.end());
  std::sort(sorted.begin(), sorted.end());
  const double tref = sorted[n / 2];
  // 初值：取自 power_rune.jsonc 的初始化运动方程
  double p[5] = {-0.3322528, 0.3322528, 1.1775, 0.0, 1.942};
  // C 初值取最接近 tref 的样本值
  double best = 1e18;
  for (std::size_t i = 0; i < n; ++i) { const double d = std::fabs(t[i] - tref); if (d < best) { best = d; p[3] = y[i]; } }
  ceres::Problem problem;
  for (std::size_t i = 0; i < n; ++i) {
    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<CeresBigResidual, 1, 5>(new CeresBigResidual(t[i], y[i], tref)),
        nullptr, p);
  }
  problem.SetParameterLowerBound(p, 4, cfg.omega_lower);
  problem.SetParameterUpperBound(p, 4, cfg.omega_upper);
  ceres::Solver::Options opt; ceresSolve(problem, opt);
  m.A = p[0]; m.B = p[1]; m.b = p[2]; m.C = p[3]; m.w = p[4]; m.tref = tref; m.valid = true;
  return m;
}

inline SmallModel ceresFitSmall(const std::vector<double>& t, const std::vector<double>& y) {
  SmallModel m;
  const std::size_t n = t.size();
  if (n < 2) return m;
  std::vector<double> sorted(t.begin(), t.end());
  std::sort(sorted.begin(), sorted.end());
  const double tref = sorted[n / 2];
  double p[2] = {1.0472, 0.0};
  double best = 1e18;
  for (std::size_t i = 0; i < n; ++i) { const double d = std::fabs(t[i] - tref); if (d < best) { best = d; p[1] = y[i]; } }
  // 初值斜率用首尾差分
  if (n >= 2 && t.back() != t.front()) p[0] = (y.back() - y.front()) / (t.back() - t.front());
  ceres::Problem problem;
  for (std::size_t i = 0; i < n; ++i) {
    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<CeresSmallResidual, 1, 2>(new CeresSmallResidual(t[i], y[i], tref)),
        nullptr, p);
  }
  ceres::Solver::Options opt; ceresSolve(problem, opt);
  m.k = p[0]; m.c = p[1]; m.tref = tref; m.valid = true;
  return m;
}

}  // namespace rune
