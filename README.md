# 能量机关转动角度预测评估（数据采集）

本工作空间用于**评估能量机关（子弹机关 / Rune）转动角度预测方法**。它通过
Daedalus 学习版模拟器的真值 SDK，采集小符/大符旋转角度的真值序列，并在此基础上
合成"带观测误差"的序列，供后续用**滑动窗口预测**方法做离线对比评估。

模拟器（`../完整版/`）只负责提供数据；本工作空间只负责采集、造观测、评估。

---

## 1. 目录结构

```
预测评估/
├── CMakeLists.txt              # 构建配置（链接 ../../完整版/sdk）
├── README.md
├── src/
│   └── capture_rune_angles.cpp # 采集工具：读真值 + 造观测误差 + 写序列
├── scripts/
│   ├── build.sh                # 构建采集工具
│   ├── run_capture.sh          # 一键采集（等周期 5ms + 逐帧两套）
│   └── eval_example.py         # 下游示例：滑动窗口预测评估
└── data/
    ├── wallclock_5ms/          # 按墙钟每 5ms 读一次"当前帧真值"（1000 点）
    │   ├── rune_small_sequences.csv
    │   ├── rune_large_sequences.csv
    │   └── dataset_manifest.json
    └── per_frame/              # 逐帧（事件驱动，每个新真值帧一点，1000 点）
        ├── rune_small_sequences.csv
        ├── rune_large_sequences.csv
        └── dataset_manifest.json
```

---

## 2. 数据来源（SDK）

采集工具只使用模拟器公开的同曝光真值接口：

* `TalosMetadataMapping::open(<ipc>/talos_ipc_meta)` + `TalosMetadataReader`：读取
  `GroundTruthBatch`，其中 `runes[i].current_angle` 即能量机关当前转角。
* `ContestClient::selectScene(Energy)` + `setRuneScenario(...)`：把能量机关设置成
  小符 / 大符的**规则驱动旋转（RuleDriven）**。
* 采集的是**红方符**（`team=0`，可用 `--team blue` 改）。

> `current_angle` 单位是**弧度**，取值落在 `(-2π, 2π]`，越界时按 **4π** 回绕
> （已实测确认：`+2π`→`-2π`）。因此每个角度序列都额外给出连续（展开）角度列
> `*_unwrap_rad`，供不做回绕处理的预测方法直接使用。

---

## 3. 采集方式（两种）

两种方式都固定采集 **1000 点 / 每条序列**：

| 数据集 | 触发方式 | 采样周期 | 说明 |
|---|---|---|---|
| `wallclock_5ms/` | 墙钟截止时间 | **严格按 5ms**（`sleep_until`） | 即需求所说"每 5ms 读当前帧真值"。 |
| `per_frame/`    | 事件驱动 | 每来一个新真值帧采 1 点 | 真实曝光率（本机实测 ≈ **29 Hz / 34ms**）。 |

**为什么要两套？** 学习版 SDK 的真值批是按**曝光帧**提交的，不是按 250Hz 物理步长。
在本机这次采集里，真值实际更新率约 **29 Hz（≈34ms/帧）**。所以：

* 严格 5ms 采 1000 点 = 5s，但真值其实只变了约 141 次（其余为重复值，构成阶梯）。
* 5ms 序列里重复与否可由 `sim_frame_seq` 列判断——同一帧号即重复。
* `per_frame` 序列的 1000 点则是 **1000 个互不相同的真值帧**（帧间隔非严格等周期
  → 这正是需求里"存另一个相对时间序列"要解决的问题）。

如果后续模拟器跑在更高帧率（真实 GPU、更高 FPS）上，5ms 序列的重复会自然减少。

### 时间戳

每条序列都带**两个相对时间**（都从本段采样起点 t=0 起算）：

* `t_wall_s`：真实墙钟相对时间（秒）——实际采样时刻，反映抖动/不等周期；
* `t_sim_s` ：仿真真值相对时间（秒）——来自 `GroundTruthBatch::timestamp_ns`。

外加 `sim_frame_seq`：该点对应的仿真帧号（判断是否为同一帧/重复）。

---

## 4. 观测误差（合成）

在真值角度上叠加噪声得到观测序列（叠加后按 4π 回绕，与真值同表示）：

* **高斯噪声**：`mean=0`，`sigma = --gauss-deg`（默认 **0.5°**）；
* **平均（均匀）分布噪声**：`U(-a, +a)`，`a = --uniform-deg`（默认 **0.2°**）；
* （可选）**合成噪声**：高斯 + 均匀（`--with-combined`）。

三种噪声对每条序列使用**同一随机种子**（`--seed`，默认 20261013），可复现。

### 序列数量

* 每模式（小符 / 大符）：真值 + 高斯观测 + 均匀观测 = **3** 条。
* 两模式共 **6** 条（需求下限）。
* 打开 `--with-combined` 再各加一条合成观测 → 两模式共 **8** 条。

当前 `data/` 下的两套数据集均使用 `--with-combined`，即**每模式 4 条、共 8 条**。
若只要需求里的 6 条，忽略 CSV 中的 `angle_obs_combined_*` 三列即可。

---

## 5. 文件格式（`rune_<mode>_sequences.csv`）

每行一个采样点，列如下（角度同时给弧度 `rad` 与角度 `deg`）：

| 列 | 含义 |
|---|---|
| `sample_index` | 采样序号 0..999 |
| `t_wall_s` | 墙钟相对时间(秒) |
| `t_sim_s` | 仿真真值相对时间(秒) |
| `sim_frame_seq` | 对应仿真帧号 |
| `angle_truth_rad` / `_deg` | **真值**角度（原始回绕表示） |
| `angle_truth_unwrap_rad` | 真值**连续展开**角度 |
| `angle_obs_gauss_rad` / `_deg` | 高斯噪声观测（回绕） |
| `angle_obs_gauss_unwrap_rad` | 高斯观测展开 |
| `angle_obs_uniform_rad` / `_deg` | 均匀噪声观测（回绕） |
| `angle_obs_uniform_unwrap_rad` | 均匀观测展开 |
| `angle_obs_combined_rad` / `_deg` | 高斯+均匀观测（回绕） |
| `angle_obs_combined_unwrap_rad` | 合成观测展开 |

`dataset_manifest.json` 用机器可读方式列出每条序列的模式、文件、列名、噪声参数。

---

## 6. 构建与运行

```bash
cd 预测评估
./scripts/build.sh                       # 生成 build/capture_rune_angles
# 前提：模拟器已在运行（例如 ../完整版/daedalus-learning.sh start --scene energy）

# 一键采集两套（小符+大符，各 1000 点）
./scripts/run_capture.sh

# 或手动采集：
./build/capture_rune_angles --out data/wallclock_5ms --samples 1000 --period-ms 5 --with-combined
./build/capture_rune_angles --out data/per_frame     --samples 1000 --on-new-frame --with-combined
```

常用参数：

```
--ipc-dir PATH       真值 IPC 目录（默认 $XDG_RUNTIME_DIR/daedalus-learning-<uid>）
--out DIR            输出目录
--samples N          每序列点数（默认 1000）
--period-ms X        等周期采样周期（默认 5）
--on-new-frame       改为逐帧事件驱动采样
--team red|blue      采红/蓝方符（默认 red）
--gauss-deg X        高斯噪声标准差(度)（默认 0.5）
--uniform-deg X      均匀噪声半宽(度)（默认 0.2）
--with-combined      追加合成噪声序列
--modes small,large  采集哪些模式
--spinup-ms X        设置工况后等待稳定(ms)（默认 1500）
--seed N             随机种子
--no-scene-control   只读真值、不改场景（真值已在旋转时用；无需连网）
```

> 采集工具通过 UDP 场景控制设置小/大符。若运行环境不允许连本地端口，可先在能连网
> 的终端执行 `../完整版/tools/switch_rune small`（或 `large`），再用
> `--no-scene-control --modes small` 只读采集。

---

## 7. 下游：滑动窗口预测评估

`scripts/eval_example.py` 演示如何读取序列并做滑动窗口预测对比：

```bash
# 用最近 8 个观测点，预测 5 个点之后的真值角度；对比 保持/线性/二次 三种方法
python3 scripts/eval_example.py --csv data/per_frame/rune_small_sequences.csv --window 8 --horizon 5
python3 scripts/eval_example.py --data data/per_frame --window 8 --horizon 5   # 小符+大符一起
```

它对每条带误差的观测序列（`*_unwrap_rad`）滑动取窗，预测若干点后的
`angle_truth_unwrap_rad`，输出 RMSE/MAE（度）。这是"评估框架"的最小示例，后续可在
此基础上接入更多方法（匀速运动模型、正弦速度模型、Kalman/alpha-beta、多项式/拟合、
学习类方法等），并改变窗口长度 `--window` 与预测步长 `--horizon` 做对比。

`per_frame` 序列上的一次示例结果（window=8, horizon=5）：

| 序列 | persistence | linear | quadratic |
|---|---|---|---|
| 小符 · 高斯观测 | 10.28 / 10.26 | **0.70** / 0.58 | 2.65 / 2.12 |
| 小符 · 均匀观测 | 10.24 / 10.23 | **0.22** / 0.17 | 0.75 / 0.60 |
| 大符 · 高斯观测 | 11.80 / 10.92 | **2.42** / 1.76 | 3.11 / 2.39 |
| 大符 · 均匀观测 | 11.80 / 10.93 | **2.31** / 1.55 | 1.77 / 1.06 |

（RMSE°/MAE°；horizon 以"点"为单位，`per_frame` 下 1 点 ≈ 33ms。）

---

## 8. 注意事项

* 真值/SDK 细节以 `../完整版/docs/` 与 `../完整版/README_ZH.md` 为准。
* 学习版**不用于比赛**；本工作空间只做算法评估。
* 采样期间请勿手动改变小/大符工况，否则序列会出现非预期的角度跳变。
* `current_angle` 的 4π 回绕由 `*_unwrap_rad` 列处理；若你的方法直接吃回绕值，
  需自行处理 `±2π` 附近的跳变。
## 9. 预测方法评估（EKF / LM / Ceres）

在采集数据的基础上，评估三种转角预测方法，详见 **[方法与结果.md](方法与结果.md)**：

```bash
./scripts/build.sh            # 同时构建 capture_rune_angles 与 predict_eval
./scripts/run_eval.sh         # 四数据集 × 三方法 × 多窗口/时域 -> results/metrics_all.csv
python3 scripts/plot_results.py   # -> results/rmse_vs_horizon.svg
```

* 运动规律：小符恒定 60°/s；大符 `φ=Acos(ωt)+Bsin(ωt)+bt+C`，
  `b=2.090−a`、`ω∈[1.884,2.0]`（来源 `RP-26Rune/.../power_rune_global.hpp`）。
* 代码：`src/ekf_predictor.hpp`（移植 buff_predict）、`src/lm_predictor.hpp`
  （移植 LM+IRLS）、`src/ceres_predictor.hpp`（Ceres 曲线拟合）。
* Ceres 2.2.0 已 vendored 到 `third_party/ceres-root/`，无需系统安装。
