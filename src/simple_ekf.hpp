// simple_ekf.hpp —— 与 sp_vision_25/tools/extended_kalman_filter 等价的极简 EKF
#pragma once
#include <Eigen/Dense>
#include <functional>

namespace rune {

class SimpleEKF {
 public:
  Eigen::VectorXd x;
  Eigen::MatrixXd P;

  SimpleEKF(const Eigen::VectorXd& x0, const Eigen::MatrixXd& P0)
      : x(x0), P(P0), I_(Eigen::MatrixXd::Identity(x0.rows(), x0.rows())) {}

  void predict(const Eigen::MatrixXd& F, const Eigen::MatrixXd& Q) {
    P = F * P * F.transpose() + Q;
    x = F * x;
  }
  void predict(const Eigen::MatrixXd& F, const Eigen::MatrixXd& Q,
               const std::function<Eigen::VectorXd(const Eigen::VectorXd&)>& f) {
    P = F * P * F.transpose() + Q;
    x = f(x);
  }
  const Eigen::VectorXd& update(const Eigen::VectorXd& z, const Eigen::MatrixXd& H,
                                const Eigen::MatrixXd& R) {
    Eigen::MatrixXd K = P * H.transpose() * (H * P * H.transpose() + R).inverse();
    P = (I_ - K * H) * P * (I_ - K * H).transpose() + K * R * K.transpose();
    x = x + K * (z - H * x);
    return x;
  }

 private:
  Eigen::MatrixXd I_;
};

}  // namespace rune
