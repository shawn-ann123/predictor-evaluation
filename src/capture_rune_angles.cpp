// capture_rune_angles.cpp
// -----------------------------------------------------------------------------
// 采集能量机关（小符 Small / 大符 Large）旋转角度的真值序列，并合成带观测误差
// 的序列，用于后续滑动窗口角度预测方法的离线评估。
//
// 数据来源：Daedalus 学习版模拟器 Talos 真值 SDK（shared memory: talos_ipc_meta）。
//   * 每 period_ms 毫秒读取一次"当前帧"能量机关转角真值 current_angle。
//   * 若无法做到严格等周期采样，则在文件里额外记录真实墙钟相对时间 t_wall_s
//     与仿真真值相对时间 t_sim_s（两列时间），并记录对应帧号 sim_frame_seq。
//
// 观测误差（在真值上叠加，得到两条（或三条）带误差的观测序列）：
//   * 高斯噪声   mean=0, sigma = --gauss-deg   （度）
//   * 平均(均匀)分布噪声  ~ U(-a, a), a = --uniform-deg （度）
//   * 可选：高斯 + 均匀 合成噪声（--with-combined），得到第 4 条观测序列。
//
// 对小符、大符各采一组 => 默认 6 条序列（2 模式 × {真值, 高斯, 均匀}），
// 打开 --with-combined 则 8 条。
//
// 单位约定：SDK 原生为弧度，文件同时给出 rad 与 deg 两套列。模拟器的
// current_angle 落在 (-2π, 2π]，越界时按 4π 回绕（已实测确认）；因此每个
// 角度序列都额外给出 *_unwrap_rad 连续（展开）角度列。
// -----------------------------------------------------------------------------
#include <daedalus_sim_sdk/contest_client.hpp>
#include <daedalus_sim_sdk/talos_metadata_reader.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace daedalus::sim::sdk::v1;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kFourPi = 4.0 * kPi;
constexpr double kRadToDeg = 180.0 / kPi;

std::string defaultIpcDir() {
  const char* xdg = std::getenv("XDG_RUNTIME_DIR");
  const std::string base = xdg != nullptr ? xdg : "/tmp";
  return base + "/daedalus-learning-" + std::to_string(::getuid());
}

struct Options {
  std::string ipc_dir = defaultIpcDir();
  std::string out_dir = "data";
  int samples = 1000;             // 每序列样本数
  double period_ms = 5.0;         // 名义采样周期 (ms)
  int team = 0;                   // 0=红, 1=蓝
  double gauss_sigma_deg = 0.5;   // 高斯噪声标准差 (度)
  double uniform_deg = 0.2;       // 均匀噪声半宽 (度)
  bool with_combined = false;     // 是否额外生成 高斯+均匀 合成序列
  double spinup_ms = 1500.0;      // 设置工况后等待稳定 (ms)
  std::uint64_t seed = 20261013ULL;
  std::string modes = "small,large";
  bool no_scene_control = false;  // 只读，不改场景（真值已在旋转时使用）
  bool on_new_frame = false;      // 事件驱动：每出现新真值帧采一点（非等周期）
  std::string tag;                // 输出文件名附加标签（可选）
};

void usage() {
  std::printf(
      "用法: capture_rune_angles [选项]\n"
      "  --ipc-dir PATH        真值 IPC 目录 (默认 %s/daedalus-learning-<uid>)\n"
      "  --out DIR             输出目录 (默认 data)\n"
      "  --samples N           每序列样本数 (默认 1000)\n"
      "  --period-ms X         采样周期 ms (默认 5.0)\n"
      "  --team red|blue       采集哪个符 (默认 red)\n"
      "  --gauss-deg X         高斯噪声标准差(度) (默认 0.5)\n"
      "  --uniform-deg X       均匀噪声半宽(度) (默认 0.2)\n"
      "  --with-combined       额外生成 高斯+均匀 合成噪声序列 (默认关闭)\n"
      "  --modes LIST          采集模式, 逗号分隔: small,large (默认 small,large)\n"
      "  --spinup-ms X         设置工况后等待稳定时间 ms (默认 1500)\n"
      "  --seed N              随机种子 (默认 20261013)\n"
      "  --no-scene-control    不通过场景控制设置工况, 只读当前真值\n"
      "  --on-new-frame        改为事件驱动: 每出现一帧新真值采一点(不做5ms等周期)\n"
      "  --tag S               输出文件名附加标签\n"
      "  --help\n",
      "/tmp");
}

bool parseOptions(int argc, char** argv, Options* o) {
  auto need = [&](int& i) -> const char* {
    if (i + 1 >= argc) {
      std::fprintf(stderr, "缺少参数值: %s\n", argv[i]);
      std::exit(2);
    }
    return argv[++i];
  };
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--ipc-dir") o->ipc_dir = need(i);
    else if (a == "--out") o->out_dir = need(i);
    else if (a == "--samples") o->samples = std::atoi(need(i));
    else if (a == "--period-ms") o->period_ms = std::atof(need(i));
    else if (a == "--team") {
      const std::string t = need(i);
      o->team = (t == "blue" || t == "1") ? 1 : 0;
    } else if (a == "--gauss-deg") o->gauss_sigma_deg = std::atof(need(i));
    else if (a == "--uniform-deg") o->uniform_deg = std::atof(need(i));
    else if (a == "--with-combined") o->with_combined = true;
    else if (a == "--modes") o->modes = need(i);
    else if (a == "--spinup-ms") o->spinup_ms = std::atof(need(i));
    else if (a == "--seed") o->seed = static_cast<std::uint64_t>(std::strtoull(need(i), nullptr, 10));
    else if (a == "--no-scene-control") o->no_scene_control = true;
    else if (a == "--on-new-frame") o->on_new_frame = true;
    else if (a == "--tag") o->tag = need(i);
    else if (a == "--help" || a == "-h") { usage(); return false; }
    else { std::fprintf(stderr, "未知参数: %s\n", a.c_str()); usage(); std::exit(2); }
  }
  return true;
}

const char* modeName(RuneMode m) { return m == RuneMode::Large ? "large" : "small"; }
int modeCode(RuneMode m) { return m == RuneMode::Large ? 1 : 0; }

// (-2π, 2π] 4π 回绕（与模拟器 current_angle 表示一致）。
double wrapFourPi(double a) {
  while (a > kTwoPi) a -= kFourPi;
  while (a <= -kTwoPi) a += kFourPi;
  return a;
}

// 从回绕表示展开为连续角度：逐步做 2π 整数倍最小修正。
struct Unwrapped {
  std::vector<double> raw;     // 原始（回绕）表示
  std::vector<double> unwrap;  // 连续展开表示
};

Unwrapped unwrapSeries(const std::vector<double>& raw) {
  Unwrapped u;
  u.raw = raw;
  u.unwrap.resize(raw.size());
  if (raw.empty()) return u;
  double prev = raw.front();
  double acc = raw.front();
  u.unwrap.front() = acc;
  for (std::size_t i = 1; i < raw.size(); ++i) {
    double step = raw[i] - prev;
    step -= kTwoPi * std::round(step / kTwoPi);
    acc += step;
    u.unwrap[i] = acc;
    prev = raw[i];
  }
  return u;
}

struct Sample {
  double t_wall_s = 0.0;
  double t_sim_s = 0.0;
  std::uint64_t frame_seq = 0;
  double truth_rad = 0.0;  // 原始（回绕）表示
};

const GroundTruthRune* findRune(const GroundTruthBatch& b, int team, int want_mode) {
  for (std::uint32_t i = 0; i < b.rune_count; ++i) {
    const GroundTruthRune& r = b.runes[i];
    if (static_cast<int>(r.team) != team) continue;
    if (want_mode >= 0 && static_cast<int>(r.rune_mode) != want_mode) continue;
    return &r;
  }
  return nullptr;
}

// 采集一段角度序列。
//   on_new_frame=false（默认）：按周期 period_ms 的墙钟截止时间采样（等周期，读到
//     的是最近一次已提交真值，可能重复；重复与否可由 sim_frame_seq 列判断）。
//   on_new_frame=true：事件驱动，每出现一帧新的真值就采一点，得到长度 samples 的
//     逐帧序列（帧间隔由模拟器曝光率决定，通常非严格等周期 -> 看 t_wall_s）。
bool captureSeries(TalosMetadataReader& reader, const Options& o, int want_mode,
                   std::vector<Sample>* out, std::string* err) {
  const auto wait_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  const GroundTruthRune* first = nullptr;
  while (std::chrono::steady_clock::now() < wait_deadline) {
    const auto tr = reader.readLatestGroundTruth();
    if (tr.ok()) {
      const GroundTruthRune* r = findRune(*tr.value, o.team, want_mode);
      if (r != nullptr) { first = r; break; }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (first == nullptr) {
    *err = "未在真值中找到匹配的符(检查场景/模式/队伍或 DAEDALUS_LEARNING_TRUTH)";
    return false;
  }

  out->clear();
  out->reserve(o.samples);
  const auto t_wall0 = std::chrono::steady_clock::now();
  const std::uint64_t sim0 = first->timestamp_ns;
  const auto period = std::chrono::duration<double, std::milli>(o.period_ms);

  auto makeSample = [&](const GroundTruthRune& r, std::chrono::steady_clock::time_point t_read) {
    Sample s;
    s.t_wall_s = std::chrono::duration<double>(t_read - t_wall0).count();
    s.t_sim_s = (r.timestamp_ns >= sim0)
                    ? (static_cast<double>(r.timestamp_ns - sim0) * 1e-9)
                    : 0.0;
    s.frame_seq = r.frame_seq;
    s.truth_rad = r.current_angle;
    return s;
  };

  if (o.on_new_frame) {
    std::uint64_t last_frame = 0;
    bool have_last = false;
    while (static_cast<int>(out->size()) < o.samples) {
      const auto tr = reader.readLatestGroundTruth();
      if (tr.ok()) {
        const GroundTruthRune* r = findRune(*tr.value, o.team, want_mode);
        if (r != nullptr && (!have_last || r->frame_seq != last_frame)) {
          last_frame = r->frame_seq;
          have_last = true;
          out->push_back(makeSample(*r, std::chrono::steady_clock::now()));
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
    return true;
  }

  for (int i = 0; i < o.samples; ++i) {
    std::this_thread::sleep_until(t_wall0 + period * i);
    const auto t_read = std::chrono::steady_clock::now();
    const auto tr = reader.readLatestGroundTruth();
    if (!tr.ok()) { *err = "读取真值失败: " + tr.status.message; return false; }
    const GroundTruthRune* r = findRune(*tr.value, o.team, want_mode);
    if (r == nullptr) { *err = "采样过程中丢失目标符"; return false; }
    out->push_back(makeSample(*r, t_read));
  }
  return true;
}

void writeModeCsv(const Options& o, RuneMode mode, const std::vector<Sample>& s,
                  const std::vector<double>& truth_raw,
                  const std::vector<double>& gauss_raw,
                  const std::vector<double>& uniform_raw,
                  const std::vector<double>& combined_raw,
                  const std::string& path_prefix) {
  const Unwrapped truth = unwrapSeries(truth_raw);
  const Unwrapped gauss = unwrapSeries(gauss_raw);
  const Unwrapped unif = unwrapSeries(uniform_raw);
  const Unwrapped comb = o.with_combined ? unwrapSeries(combined_raw) : Unwrapped{};

  const std::string path = path_prefix + "_sequences.csv";
  std::ofstream f(path);
  f << std::setprecision(10);
  f << "sample_index,t_wall_s,t_sim_s,sim_frame_seq,"
       "angle_truth_rad,angle_truth_deg,angle_truth_unwrap_rad,"
       "angle_obs_gauss_rad,angle_obs_gauss_deg,angle_obs_gauss_unwrap_rad,"
       "angle_obs_uniform_rad,angle_obs_uniform_deg,angle_obs_uniform_unwrap_rad";
  if (o.with_combined) {
    f << ",angle_obs_combined_rad,angle_obs_combined_deg,angle_obs_combined_unwrap_rad";
  }
  f << "\n";
  for (std::size_t i = 0; i < s.size(); ++i) {
    f << i << ',' << s[i].t_wall_s << ',' << s[i].t_sim_s << ',' << s[i].frame_seq
      << ',' << truth.raw[i] << ',' << truth.raw[i] * kRadToDeg << ',' << truth.unwrap[i]
      << ',' << gauss.raw[i] << ',' << gauss.raw[i] * kRadToDeg << ',' << gauss.unwrap[i]
      << ',' << unif.raw[i] << ',' << unif.raw[i] * kRadToDeg << ',' << unif.unwrap[i];
    if (o.with_combined) {
      f << ',' << comb.raw[i] << ',' << comb.raw[i] * kRadToDeg << ',' << comb.unwrap[i];
    }
    f << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  Options o;
  if (!parseOptions(argc, argv, &o)) return 0;

  std::error_code ec;
  fs::create_directories(o.out_dir, ec);
  if (ec) { std::fprintf(stderr, "无法创建输出目录 %s: %s\n", o.out_dir.c_str(), ec.message().c_str()); return 1; }

  // 真值读取：始终使用独立只读映射。
  TalosMetadataMapping mapping;
  const ClientStatus open_status = mapping.open(o.ipc_dir + "/talos_ipc_meta");
  if (!open_status.ok()) {
    std::fprintf(stderr, "打开真值 IPC 失败 (%s): %s\n", (o.ipc_dir + "/talos_ipc_meta").c_str(),
                 open_status.message.c_str());
    return 1;
  }
  auto reader_res = mapping.reader();
  if (!reader_res.ok()) {
    std::fprintf(stderr, "创建真值 reader 失败: %s\n", reader_res.status.message.c_str());
    return 1;
  }
  TalosMetadataReader& reader = *reader_res.value;

  // 场景控制（可选）：通过 SDK 设置小符/大符规则驱动旋转。
  std::unique_ptr<ContestClient> client;
  if (!o.no_scene_control) {
    ContestClientOptions copt;
    copt.ipc_directory = o.ipc_dir;
    client = std::make_unique<ContestClient>(copt);
    const ClientStatus conn = client->connect();
    if (!conn.ok()) {
      std::fprintf(stderr, "连接模拟器失败: %s\n", conn.message.c_str());
      std::fprintf(stderr, "提示：若只需读取，请用 --no-scene-control。\n");
      return 1;
    }
    const auto scene = client->selectScene(ContestScene::Energy);
    if (!scene.ok()) {
      std::fprintf(stderr, "切换到能量机关场景失败: %s\n", scene.status.message.c_str());
      return 1;
    }
    std::printf("scene=energy message=%s\n", scene.value->message.c_str());
  }

  std::vector<RuneMode> modes;
  if (o.modes.find("small") != std::string::npos) modes.push_back(RuneMode::Small);
  if (o.modes.find("large") != std::string::npos) modes.push_back(RuneMode::Large);
  if (modes.empty()) { std::fprintf(stderr, "--modes 为空\n"); return 2; }

  std::mt19937_64 rng(o.seed);
  std::normal_distribution<double> gauss(0.0, o.gauss_sigma_deg / kRadToDeg);
  std::uniform_real_distribution<double> unif(-o.uniform_deg / kRadToDeg, o.uniform_deg / kRadToDeg);

  const int n_series = o.with_combined ? 8 : 6;
  std::printf("计划采集 %zu 个模式 x %d 条序列 = %d 条（每序列 %d 点, 周期 %.3f ms）\n",
              modes.size(), o.with_combined ? 4 : 3, n_series * (int)modes.size(), o.samples, o.period_ms);

  for (RuneMode mode : modes) {
    if (!o.no_scene_control) {
      RuneScenario sc;
      sc.mode = mode;
      sc.motion = RuneMotion::RuleDriven;
      sc.red_face_direction = RuneDirection::Clockwise;
      sc.leaf_states.clear();
      const auto resp = client->setRuneScenario(sc);
      if (!resp.ok()) {
        std::fprintf(stderr, "setRuneScenario(%s) 失败: %s\n", modeName(mode), resp.status.message.c_str());
        return 1;
      }
      std::printf("[%s] 工况已设置: %s\n", modeName(mode), resp.value->message.c_str());
      std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(o.spinup_ms));
    }

    std::vector<Sample> s;
    std::string err;
    if (!captureSeries(reader, o, modeCode(mode), &s, &err)) {
      std::fprintf(stderr, "[%s] 采集失败: %s\n", modeName(mode), err.c_str());
      return 1;
    }

    // 叠加观测噪声（在真值上叠加后按 4π 回绕）。
    std::vector<double> truth_raw(s.size()), gauss_raw(s.size()), unif_raw(s.size()), comb_raw(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
      const double g = gauss(rng);
      const double u = unif(rng);
      truth_raw[i] = wrapFourPi(s[i].truth_rad);
      gauss_raw[i] = wrapFourPi(s[i].truth_rad + g);
      unif_raw[i] = wrapFourPi(s[i].truth_rad + u);
      comb_raw[i] = wrapFourPi(s[i].truth_rad + g + u);
    }

    const Unwrapped t = unwrapSeries(truth_raw);
    double tmin = t.unwrap.front(), tmax = t.unwrap.front();
    for (double v : t.unwrap) { tmin = std::min(tmin, v); tmax = std::max(tmax, v); }
    double sum_period = 0.0, max_period = 0.0;
    for (std::size_t i = 1; i < s.size(); ++i) {
      const double d = s[i].t_wall_s - s[i - 1].t_wall_s;
      sum_period += d; max_period = std::max(max_period, d);
    }
    const double avg_period_ms = s.size() > 1 ? 1000.0 * sum_period / (s.size() - 1) : 0.0;

    std::string prefix = o.out_dir + "/rune_" + modeName(mode);
    if (!o.tag.empty()) prefix += "_" + o.tag;
    writeModeCsv(o, mode, s, truth_raw, gauss_raw, unif_raw, comb_raw, prefix);

    std::printf("[%s] 写出 %s_sequences.csv  | 点=%zu 平均周期=%.3fms 最大间隔=%.3fms "
                "真值展开跨度=%.3f rad (%.1f°)\n",
                modeName(mode), prefix.c_str(), s.size(), avg_period_ms, max_period * 1000.0,
                tmax - tmin, (tmax - tmin) * kRadToDeg);
  }

  // 写出数据集清单。
  std::ofstream mf(o.out_dir + "/dataset_manifest.json");
  mf << std::setprecision(10);
  mf << "{\n";
  mf << "  \"tool\": \"capture_rune_angles\",\n";
  mf << "  \"ipc_dir\": \"" << o.ipc_dir << "\",\n";
  mf << "  \"samples_per_series\": " << o.samples << ",\n";
  mf << "  \"nominal_period_ms\": " << o.period_ms << ",\n";
  mf << "  \"team\": " << (o.team == 0 ? "\"red\"" : "\"blue\"") << ",\n";
  mf << "  \"noise\": {\n";
  mf << "    \"gaussian_sigma_deg\": " << o.gauss_sigma_deg << ",\n";
  mf << "    \"uniform_halfwidth_deg\": " << o.uniform_deg << ",\n";
  mf << "    \"combined_enabled\": " << (o.with_combined ? "true" : "false") << ",\n";
  mf << "    \"seed\": " << o.seed << "\n";
  mf << "  },\n";
  mf << "  \"series_count\": " << (o.with_combined ? 8 : 6) << ",\n";
  mf << "  \"series\": [\n";
  const char* cols[4] = {"angle_truth", "angle_obs_gauss", "angle_obs_uniform", "angle_obs_combined"};
  const int ncol = o.with_combined ? 4 : 3;
  for (std::size_t mi = 0; mi < modes.size(); ++mi) {
    for (int ci = 0; ci < ncol; ++ci) {
      const bool last = (mi + 1 == modes.size()) && (ci + 1 == ncol);
      mf << "    {\"mode\": \"" << modeName(modes[mi]) << "\", \"series\": \"" << cols[ci]
         << "\", \"file\": \"rune_" << modeName(modes[mi]) << (o.tag.empty() ? "" : ("_" + o.tag))
         << "_sequences.csv\", \"angle_column\": \"" << cols[ci] << "_rad\", \"unwrap_column\": \""
         << ((ci == 0) ? std::string("angle_truth_unwrap_rad") : (std::string(cols[ci]) + "_unwrap_rad"))
         << "\"}" << (last ? "" : ",") << "\n";
    }
  }
  mf << "  ]\n}\n";

  std::printf("完成：输出目录 %s\n", o.out_dir.c_str());
  return 0;
}
