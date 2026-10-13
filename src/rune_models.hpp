// rune_models.hpp —— 能量机关转角变化公式（大小符）+ 角度回绕/展开工具
//
// 结论（已用模拟器真值核验）：
//   小符(Small)：角速度恒定  Ω_small = π/3 rad/s = 60°/s
//       φ(t) = φ0 + s·Ω_small·t ,   s = ±1 (顺/逆时针)
//
//   大符(Large)：角速度呈正弦变化，最大 120°/s = 2.09 rad/s
//       v(t) = (2.09 - a) + a·sin(ω·t + φ)     a∈[0.78,1.045], ω∈[1.884,2.0]
//       φ(t) = C + (2.09 - a)·t - (a/ω)·cos(ω·t + φ)
//            = A·cos(ω·t) + B·sin(ω·t) + b·t + C
//       其中 A = -(a/ω)cosφ, B = (a/ω)sinφ, b = 2.09 - a, a = ω·sqrt(A²+B²)
//
// 模拟器在线真值还直接给出大符模型参数：sin_amplitude=a, sin_omega=ω,
// sin_phase=φ, sin_offset=2.09-a，以及 relative_time=t。实测该 (a,ω) 会周期性
// 重新随机并重置 relative_time（本机约每 2.5s 一次），故滑动窗口内应视为分段常数。
//
// 角度表示：SDK current_angle 落在 (-2π, 2π]，越界按 4π 回绕；*_unwrap_rad 为连续展开。
#pragma once
#include <cmath>
#include <vector>

namespace rune {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kSmallOmega = kPi / 3.0;   // 小符恒定角速度 (rad/s)
constexpr double kBigSpeedMax = 2.09;       // 大符最大转速 (rad/s) = 120°/s
constexpr double kDeg = 180.0 / kPi;

inline double wrapFourPi(double a) {
  while (a > kTwoPi) a -= 2.0 * kTwoPi;
  while (a <= -kTwoPi) a += 2.0 * kTwoPi;
  return a;
}

// 从回绕表示展开为连续角度。
inline std::vector<double> unwrapSeries(const std::vector<double>& raw) {
  std::vector<double> u(raw.size());
  if (raw.empty()) return u;
  double prev = raw.front(), acc = raw.front();
  u.front() = acc;
  for (std::size_t i = 1; i < raw.size(); ++i) {
    double step = raw[i] - prev;
    step -= kTwoPi * std::round(step / kTwoPi);
    acc += step;
    u[i] = acc;
    prev = raw[i];
  }
  return u;
}

// 大符解析角度（由正弦转速积分）。
inline double bigAngle(double t, double a, double w, double ph, double C) {
  return C + (kBigSpeedMax - a) * t - (a / w) * std::cos(w * t + ph);
}
inline double bigSpeed(double t, double a, double w, double ph) {
  return (kBigSpeedMax - a) + a * std::sin(w * t + ph);
}
// 由 (A,B,b,C,w) 反解物理量
inline double bigAmp(double A, double B, double w) { return w * std::hypot(A, B); }

}  // namespace rune
