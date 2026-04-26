# FPGA-Litho Accelerator

**光刻仿真 FPGA 加速项目** — 基于 AMD Xilinx Vitis HLS 的 SOCS（Sum of Coherent Systems）光刻仿真算法硬件加速实现。

---

## 项目概述

本项目旨在将光刻仿真中的 SOCS 算法移植到 FPGA 平台，利用 HLS（High-Level Synthesis）技术实现高性能计算加速。项目采用 **Vitis HLS 2025.2** 开发工具链，目标平台为 **Kintex UltraScale+ (xcku5p-sfvb210-2-i)**。

### 核心算法

- **SOCS (Sum of Coherent Systems)**：光刻成像核心算法
- **TCC (Transmission Cross Coefficients)**：光学系统传输系数
- **FFT/IFFT**：频域变换核心计算单元

---

## 目录结构

```
FPGA-Litho/
├── README.md                   # 本文件 - 项目整体说明
├── verify.py                   # 验证快捷入口（调用 validation/golden/）
├── TODO.md                     # 项目任务追踪
├── .github/                    # GitHub 配置与 Skills
│   ├── copilot-instructions.md # Copilot 开发约束指南
│   └── skills/                 # HLS 开发技能定义
│       ├── hls-golden-generation/
│       ├── hls-board-validation/
│       ├── hls-csynth-verify/
│       └── hls-full-validation/
│
├── validation/                 # 【验证体系】统一的验证与测试目录
│   ├── README.md               # 验证体系总体说明
│   ├── golden/                 # Golden 数据生成（算法正确性验证）
│   │   ├── run_verification.py # 统一验证入口脚本
│   │   ├── src/                # CPU 参考实现（litho.cpp）
│   │   └── debug_nan_source.py # NaN 问题调试工具
│   └── board/                  # 板级验证（硬件功能验证）
│       ├── jtag/               # JTAG-to-AXI 验证脚本
│       ├── pcie/               # PCIe 验证（待开发）
│       ├── common/             # AXI 内存测试工具
│       └── scripts/            # 辅助脚本
│
├── source/                     # 【源代码】HLS 与主机程序
│   ├── SOCS_HLS/               # SOCS HLS IP 开发
│   │   ├── src/                # HLS kernel 源码
│   │   ├── tb/                 # HLS 测试平台
│   │   ├── script/             # 综合脚本与配置
│   │   ├── data/               # Golden 测试数据
│   │   ├── doc/                # SOCS 文档（任务清单、验证指南等）
│   │   ├── vivado_bd/          # Vivado Block Design 引用
│   │   └── versions/           # 版本迭代记录
│   ├── TCC_HLS/                # TCC HLS IP 开发（独立模块）
│   └── host/                   # 主机端程序（驱动、文件 IO）
│
├── input/                      # 【输入数据】配置与 Mask 数据
│   ├── config/                 # 光学参数配置（JSON）
│   ├── mask/                   # Mask 图像数据
│   └── BIN_Format_Specification.md
│
├── output/                     # 【输出数据】验证结果
│   └── verification/           # Golden 输出（BIN 格式）
│       ├── mskf_r.bin, mskf_i.bin  # Mask 频域数据
│       ├── scales.bin               # 特征值数组
│       ├── kernels/                 # SOCS kernel 数据
│       └── aerial_image_*.bin       # 成像结果
│
├── reference/                  # 【参考资料】
│   ├── CPP_reference/          # C++ 参考实现（完整算法）
│   │   ├── Litho-SOCS/         # SOCS 完整实现
│   │   ├── Litho-TCC/          # TCC 完整实现
│   │   └── Litho-Full/         # 全流程实现
│   ├── vitis_hls_ftt的实现/    # HLS FFT 集成模板
│   └── tcl脚本设计参考/         # TCL 脚本模板
│
└── doc/                        # 【文档】设计文档与学习笔记
    ├── 论文/                   # 学术论文相关
    └── 学习笔记/               # Vivado 流程笔记
```

---

## 核心模块说明

### 1. 验证体系 (`validation/`)

| 子目录        | 职能           | 输入                        | 输出                        | 用途                        |
| ------------- | -------------- | --------------------------- | --------------------------- | --------------------------- |
| `golden/`     | 算法正确性验证 | `input/config/*.json`       | `output/verification/*.bin` | HLS C 仿真、Co-Sim 对比基准 |
| `board/jtag/` | JTAG 硬件验证  | `output/verification/*.bin` | FPGA 寄存器值               | 板级功能验证                |
| `board/pcie/` | PCIe 高速验证  | （待开发）                  | （待开发）                  | 大数据量验证                |

**使用方式**：
```bash
# 快捷入口（根目录）
python verify.py --config input/config/config.json

# 或直接调用
python validation/golden/run_verification.py --config input/config/config.json
```

### 2. HLS 开发 (`source/SOCS_HLS/`)

| 目录             | 内容                                 | 说明            |
| ---------------- | ------------------------------------ | --------------- |
| `src/`           | `calcSOCS.cpp`, `fft_2d_forward.cpp` | HLS kernel 源码 |
| `tb/`            | `tb_calcSOCS.cpp`                    | C 测试平台      |
| `script/config/` | `hls_config_socs_full.cfg`           | HLS 综合配置    |
| `data/`          | `GOLDEN_*.md`                        | Golden 数据文档 |

**HLS 综合命令**（参考 `.github/copilot-instructions.md`）：
```bash
cd source/SOCS_HLS
v++ -c --mode hls --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp
```

### 3. 主机程序 (`source/host/`)

- `socs_host.cpp`：PCIe 驱动主机程序
- `file_io.cpp`：BIN 文件读写
- `kernel_loader.cpp`：SOCS kernel 加载

---

## 快速开始

### 环境要求

| 工具      | 版本   | 用途               |
| --------- | ------ | ------------------ |
| Vitis HLS | 2025.2 | HLS 综合           |
| Vivado    | 2025.2 | RTL 综合与板级验证 |
| Python    | 3.10+  | Golden 数据生成    |
| g++ / gcc | C++17  | CPU 参考编译       |

### 生成 Golden 数据

```bash
# 1. 编译 CPU 参考实现（需要 FFTW3、LAPACK、BLAS）
cd validation/golden/src
make

# 2. 运行验证脚本
cd ../
python run_verification.py --config ../../input/config/config.json

# 3. 检查输出
ls -lh ../../output/verification/
```

### HLS 综合流程

```bash
# 1. C 综合
cd source/SOCS_HLS
v++ -c --mode hls --config script/config/hls_config_socs_full.cfg

# 2. C/RTL 联合仿真
vitis-run --mode hls --cosim --config script/config/hls_config_socs_full.cfg

# 3. 导出 IP
vitis-run --mode hls --package --config script/config/hls_config_socs_full.cfg
```

---

## 开发约束与指南

### Copilot 开发约束

详见 `.github/copilot-instructions.md`，包含：
- HLS 命令规范（Vitis 2025.2 验证）
- Nx/Ny 动态参数计算公式
- Toolcall 命令格式约束
- Golden 数据生成流程

### Skills 技能系统

| Skill                   | 文件                                            | 用途                |
| ----------------------- | ----------------------------------------------- | ------------------- |
| `hls-golden-generation` | `.github/skills/hls-golden-generation/SKILL.md` | Golden 数据生成流程 |
| `hls-csynth-verify`     | `.github/skills/hls-csynth-verify/SKILL.md`     | C 综合与性能验证    |
| `hls-full-validation`   | `.github/skills/hls-full-validation/SKILL.md`   | 完整验证流程        |
| `hls-board-validation`  | `.github/skills/hls-board-validation/SKILL.md`  | 板级 JTAG 验证      |

---

## 关键配置参数

### 光学参数 (`input/config/config.json`)

```json
{
  "NA": 0.8,           // 数值孔径
  "lambda": 193,       // 波长 (nm)
  "sigma_inner": 0.3,  // 内部分离系数
  "sigma_outer": 0.9,  // 外部分离系数
  "Lx": 512,           // X 方向尺寸
  "Ly": 512            // Y 方向尺寸
}
```

### Nx/Ny 动态计算

$$N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$$

当前配置：$N_x \approx 4$（需调整 NA 或 Lx 以达到 HLS 目标 $N_x=16$）

---

## 项目状态

| 模块            | 状态     | 进度                                          |
| --------------- | -------- | --------------------------------------------- |
| Golden 数据生成 | ✅ 完成   | 可生成 `tmpImgp_pad32.bin`                    |
| HLS FFT Kernel  | ✅ 完成   | Fmax ≈ 273 MHz                                |
| HLS SOCS Kernel | ✅ 完成   | **v16 Block Floating Point模式验证通过**      |
| Fourier插值     | ✅ 完成   | 128×128 → 1024×1024，RMSE=5.06e-04            |
| 板级验证        | 📋 准备中 | TCL 脚本已完成                                |

### 最新成果（v16 Block Floating Point模式）

**验证日期**：2026-04-26

**关键指标**：
- **tmpImgp RMSE**: 2.93e-08（目标 < 1e-5）✅ **远超预期**
- **DSP利用率**: 46/1,368 (3%) - 相比v13降低**99.4%**
- **BRAM利用率**: 399/720 (55%) - 相比v13降低**70.8%**
- **Fmax**: 273.97 MHz

**技术突破**：
1. **Block Floating Point模式**：成功解决FFT精度问题
2. **精度提升**：RMSE从2.99e-05降至2.93e-08（提升99.7%）
3. **资源优化**：DSP从8,064降至46（降低99.4%）
4. **完整验证流程**：C Sim → C Synth → Fourier Interpolation → 可视化

**验证报告**：`output/verification/v16_validation_report.md`

**可视化结果**：`output/verification/v16_vs_golden_comparison.png`

---

## 许可证

本项目仅供学术研究使用。

---

## 联系方式

- 项目维护者：Ashington258
- GitHub：https://github.com/Ashington258/fpga-litho-accel