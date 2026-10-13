// ekf_predictor.hpp —— EKF 方法（移植自 sp_vision_25/tasks/auto_buff/buff_predict.hpp）
//   SmallEKF：小符 1 维状态（恒定角速度 π/3）
//   BigEKF  ：大符 5 维状态 [angle, spd, a, w, sita]，正弦转速模型
// predictChange(dt) 返回"未来 dt 时间内的角度增量"，未来角 = 当前滤波角 + 增量。
#pragma once
#include <Eigen/Dense>
#include <cmath>
#include "rune_models.hpp"
#include "simple_ekf.hpp"

namespace rune {

class SmallEKF {
 public:
  SmallEKF() : ekf_(Eigen::VectorXd::Zero(1), Eigen::MatrixXd::Identity(1, 1)) {
    Eigen::MatrixXd P0(1, 1); P0 << 1.0; ekf_.P = P0;
  }
  void reset(double angle, double t) {
    first_ = false; last_angle_ = angle; last_time_ = t; cw_ccw_ = 0;
    ekf_.x[0] = angle;
    ekf_.P << 1.0;
  }
  void update(double angle, double t) {
    if (first_) { reset(angle, t); return; }
    if (std::abs(angle - last_angle_) > kPi / 12) {
      for (int i = -5; i <= 5; ++i) {
        double ac = last_angle_ + i * 2 * kPi / 5;
        if (std::fabs(ac - angle) < kPi / 5) { ekf_.x[0] += i * 2 * kPi / 5; last_angle_ += i * 2 * kPi / 5; break; }
      }
    }
    if (std::abs(cw_ccw_) < 100) { if (last_angle_ > angle) cw_ccw_ -= 1; else cw_ccw_ += 1; }
    const double dt = t - last_time_;
    Eigen::MatrixXd A = Eigen::MatrixXd::Identity(1, 1);
    Eigen::MatrixXd Q = Eigen::MatrixXd::Zero(1, 1);
    const double B = kSmallOmega * (cw_ccw_ > 0 ? 1.0 : -1.0);
    ekf_.predict(A, Q, [&](const Eigen::VectorXd& x) -> Eigen::VectorXd {
      Eigen::VectorXd r(1); r << A(0, 0) * x[0] + dt * B; return r;
    });
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(1, 1);
    Eigen::MatrixXd R = Eigen::MatrixXd::Identity(1, 1); R << 0.05;
    Eigen::VectorXd z(1); z << angle;
    ekf_.update(z, H, R);
    last_time_ = t; last_angle_ = angle;
  }
  [[nodiscard]] double angle() const { return ekf_.x[0]; }
  [[nodiscard]] double predictChange(double dt) const { return (cw_ccw_ > 0 ? 1.0 : -1.0) * kSmallOmega * dt; }
  [[nodiscard]] bool ready() const { return !first_; }

 private:
  SimpleEKF ekf_;
  double last_angle_ = 0, last_time_ = 0;
  int cw_ccw_ = 0;
  bool first_ = true;
};

// 大符 EKF 可调参数（便于调快收敛）
struct BigEkfConfig {
  double x0[5] = {0.0, 1.1775, 0.9125, 1.942, 0.0};   // [angle, spd, a, w, sita]
  // 调快收敛后的默认值: 大幅增大 a/w/sita 的初始不确定度与过程噪声, 减小观测噪声
  double p0[5] = {1.0, 200.0, 200.0, 20.0, 2000.0};   // 初始协方差对角
  double q[5]  = {0.1, 0.5, 0.5, 0.05, 0.5};          // 过程噪声对角
  double r = 1e-3;                                    // 观测噪声方差
};

class BigEKF {
 public:
  explicit BigEKF(const BigEkfConfig& cfg = BigEkfConfig{})
      : cfg_(cfg), ekf_(Eigen::VectorXd::Zero(5), Eigen::MatrixXd::Identity(5, 5)) {
    setState(cfg_.x0[0]);
  }
  void reset(double angle, double t) {
    first_ = false; last_angle_ = angle; last_time_ = t; cw_ccw_ = 0;
    setState(angle);
  }
  void update(double angle, double t) {
    if (first_) { reset(angle, t); return; }
    if (std::abs(angle - last_angle_) > kPi / 12) {
      for (int i = -5; i <= 5; ++i) {
        double ac = last_angle_ + i * 2 * kPi / 5;
        if (std::fabs(ac - angle) < kPi / 5) { ekf_.x[0] += i * 2 * kPi / 5; last_angle_ += i * 2 * kPi / 5; break; }
      }
    }
    if (std::abs(cw_ccw_) < 100) { if (last_angle_ > angle) cw_ccw_ -= 1; else cw_ccw_ += 1; }
    const double dt = t - last_time_;
    const double a = ekf_.x[2], w = ekf_.x[3], sita = ekf_.x[4];
    const double sg = (cw_ccw_ > 0 ? 1.0 : -1.0);
    Eigen::MatrixXd A(5, 5);
    A << 1.0, 0.0,
        sg * (-1 / w * std::cos(sita + w * dt) + 1 / w * std::cos(sita) - dt),
        sg * (a / (w * w) * std::cos(sita + w * dt) - a / (w * w) * std::cos(sita)),
        sg * (a / w * std::sin(sita + w * dt) - a / w * std::sin(sita)),
        0.0, 0.0, std::sin(sita + w * dt) - 1, dt * a * std::cos(sita + w * dt), a * std::cos(sita + w * dt),
        0.0, 0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, dt, 1.0;
    ekf_.predict(A, Q_, [&](const Eigen::VectorXd& x) -> Eigen::VectorXd {
      Eigen::VectorXd m(5);
      m << x[0] + sg * (-a / w * std::cos(sita + w * dt) + a / w * std::cos(sita) + (kBigSpeedMax - a) * dt),
          a * std::sin(sita + w * dt) + kBigSpeedMax - a, a, w, sita + w * dt;
      return m;
    });
    Eigen::MatrixXd H(1, 5); H << 1.0, 0.0, 0.0, 0.0, 0.0;
    Eigen::MatrixXd R(1, 1); R << cfg_.r;
    Eigen::VectorXd z(1); z << angle;
    ekf_.update(z, H, R);
    last_time_ = t; last_angle_ = angle;
  }
  [[nodiscard]] double angle() const { return ekf_.x[0]; }
  [[nodiscard]] double predictChange(double dt) const {
    const double a = ekf_.x[2], w = ekf_.x[3], sita = ekf_.x[4];
    const double sg = (cw_ccw_ > 0 ? 1.0 : -1.0);
    return sg * (-a / w * std::cos(sita + w * dt) + a / w * std::cos(sita) + (kBigSpeedMax - a) * dt);
  }
  [[nodiscard]] bool ready() const { return !first_; }

 private:
  void setState(double angle) {
    Eigen::VectorXd x0(5);
    x0 << angle, cfg_.x0[1], cfg_.x0[2], cfg_.x0[3], cfg_.x0[4];
    ekf_.x = x0;
    Eigen::MatrixXd P0 = Eigen::MatrixXd::Zero(5, 5);
    for (int i = 0; i < 5; ++i) P0(i, i) = cfg_.p0[i];
    ekf_.P = P0;
    Q_ = Eigen::MatrixXd::Zero(5, 5);
    for (int i = 0; i < 5; ++i) Q_(i, i) = cfg_.q[i];
  }
  BigEkfConfig cfg_;
  SimpleEKF ekf_;
  Eigen::MatrixXd Q_;
  double last_angle_ = 0, last_time_ = 0;
  int cw_ccw_ = 0;
  bool first_ = true;
};

}  // namespace rune
