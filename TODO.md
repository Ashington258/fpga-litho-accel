
# FPGA-Litho 项目任务索引

**最后更新**: 2025-01-21

---

## 项目概述

将 `reference/CPP_reference/Litho-TCC` 和 `Litho-SOCS` 重构为 Vitis HLS IP 核。

**最新进展**: SOCS HLS源代码已完成整理，统一为 `socs_config.h` + `socs_simple.cpp/h` 架构。

---

## 独立任务清单

本项目包含两个**完全独立**的HLS重构任务，各有独立的TODO、Golden数据和测试脚本：

| 模块     | 任务清单                                                     | 状态         | 说明                |
| -------- | ------------------------------------------------------------ | ------------ | ------------------- |
| **TCC**  | [source/TCC_HLS/TCC_TODO.md](source/TCC_HLS/TCC_TODO.md)     | Phase 1 存档 | 等待DDR计算卡       |
| **SOCS** | [source/SOCS_HLS/SOCS_TODO.md](source/SOCS_HLS/SOCS_TODO.md) | Phase 0 完成 | 源码整理完成，待验证 |

---

## 模块关系

```
光源/参数 → TCC模块(FPGA) → TCC矩阵 → Host端calcKernels → SOCS核 → SOCS模块(FPGA) → 空中像
```

- **TCC模块**: FPGA端实现，Phase 1已完成存档
- **calcKernels**: Host端LAPACK实现，不在FPGA重构范围
- **SOCS模块**: FPGA端实现，当前任务

---

## 全局约束

所有通用约束见 `.github/copilot-instructions.md`，包括：

- 参考资源路径
- HLS命令规范（vitis-run）
- 数据类型定义（float / complex<float>）
- BIN文件格式
- 输出要求
- 验收标准

---

## 关键文件位置

| 类型       | TCC模块                      | SOCS模块                         |
| ---------- | ---------------------------- | -------------------------------- |
| 任务清单   | `source/TCC_HLS/TCC_TODO.md` | `source/SOCS_HLS/SOCS_TODO.md`   |
| 源码       | `source/TCC_HLS/hls/src/`    | `source/SOCS_HLS/hls/src/`       |
| Golden数据 | `source/TCC_HLS/hls/golden/` | `source/SOCS_HLS/hls/tb/golden/` |
| 测试脚本   | `source/TCC_HLS/hls/tb/`     | `source/SOCS_HLS/hls/tb/`        |

---

## 快速开始

### 验证入口 (统一)
```bash
cd /root/project/FPGA-Litho
python verify.py                      # 默认验证
python verify.py --clean              # 清理重新验证
python validation/golden/run_verification.py --debug  # 调试模式
```

### 验证输出（SOCS HLS Golden输入）

验证程序生成所有SOCS HLS所需的golden输入数据：

| 文件                                        | 用途                       |
| ------------------------------------------- | -------------------------- |
| `output/verification/mskf_r/i.bin`          | Mask频域数据（AXI-MM输入） |
| `output/verification/scales.bin`            | 特征值数组（AXI-MM输入）   |
| `output/verification/kernels/krn_*_r/i.bin` | SOCS核数据                 |
| `output/verification/image.bin`             | 空中像输出（HLS对比基准）  |

### TCC模块
```bash
cd /root/project/FPGA-Litho/source/TCC_HLS
cat TCC_TODO.md          # 查看任务
make csim                # 运行C仿真
```

### SOCS模块
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
cat SOCS_TODO.md         # 查看任务
python generate_golden.py  # 生成Golden数据
make csim                # 运行C仿真
```

---

## 当前状态

### TCC模块 (Phase 1 存档)
- ✅ C仿真通过（相对误差 < 1e-5）
- ✅ C综合完成（RTL生成）
- ⚠️ BRAM超限407%（等待DDR计算卡）

### SOCS模块 (Phase 0 完成)
- ✅ 目录结构创建
- ✅ 基础框架搭建
- ⬜ 集成hls::fft
- ⬜ 生成Golden数据

---

**维护说明**: 本索引文件仅作为入口，详细任务请查看各模块独立的TODO文件。

---

## 详细任务清单（历史参考）

> **注意**: 以下为历史任务清单，新任务请查看各模块独立TODO文件

---

### 🎯 核心设计原则（接口模块化，便于后期更换）

| 接口类型     | 当前实现                 | 后期升级路径            | 更改点               |
| ------------ | ------------------------ | ----------------------- | -------------------- |
| **参数配置** | AXI-Lite 寄存器          | JTAG/PCIe Host 写寄存器 | 只改 Vivado BD 连线  |
| **数据输入** | JTAG → BRAM → AXI-Stream | PCIe DMA → AXI-Stream   | Vivado BD 替换 IP 核 |
| **存储接口** | AXI-Master → BRAM        | AXI-Master → DDR (MIG)  | Vivado BD 连线       |
| **数据输出** | JTAG 读 BRAM             | PCIe DMA 接收           | Vivado BD 替换 IP 核 |

**关键约束**：
1. **HLS 代码不依赖数据来源**：所有输入/输出均通过 AXI-Stream，不直接访问 BRAM/DDR
2. **参数通过 AXI-Lite 传入**：不硬编码任何参数
3. **Host/Adapter 代码单独存放**：`host/` 目录，便于后期替换 JTAG → PCIe
4. **Vivado BD 示例单独存放**：`vivado_bd/` 目录，便于后期更换接口 IP 核

### 🎯 最终输出要求

**FPGA 端输出（当前重构范围）**：
| 输出项                    | 格式                             | 说明             | 用途                   |
| ------------------------- | -------------------------------- | ---------------- | ---------------------- |
| **空中像 (Aerial Image)** | float32, Lx×Ly                   | 光刻成像强度分布 | 直接可用               |
| **TCC 矩阵**              | complex float32, tccSize×tccSize | 传输交叉相关矩阵 | 供 Host 端计算 SOCS 核 |

**Host 端输出（后期实现）**：
| 输出项           | 格式                                | 说明                 | 执行位置              |
| ---------------- | ----------------------------------- | -------------------- | --------------------- |
| **SOCS Kernels** | nk 个复数核，每个 (2×Nx+1)×(2×Ny+1) | 特征值分解后的卷积核 | Host (LAPACK zheevr_) |

**输出流程**：
```
FPGA 端 (HLS):
  source/mask → calcTCC → TCC 矩阵 ──→ Host 端
                 ↓
               calcImage → 空中像 ──────→ 输出

Host 端 (CPU):
  TCC 矩阵 → calcKernels (LAPACK) → SOCS Kernels → 输出
```

**⚠️ 当前阶段（Phase 0-4）只重构 FPGA 端**：calcKernels 不在 HLS 中实现。

---

### 🎯 HLS IP 架构（接口模块化）

```
┌─────────────────────────────────────────────────────────────────┐\│                         HLS IP (tcc_top)                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐           │
│  │ AXI-Lite    │    │ AXI-Stream  │    │ AXI-Master  │           │
│  │ (参数配置)  │    │ (数据输入)  │    │ (存储接口)  │           │
│  └─────────────┘    └──────────┬─┘    └──────┬──────┘           │
│         │                   │              │                  │
│  ┌──────┴────────────────────┴──────────────┴──────┐           │
│  │                   DATAFLOW                      │           │
│  │  ┌──────────┐   ┌──────────┐   ┌──────────┐     │           │
│  │  │ calcTCC  │ → │ calcImage│ → │ 输出流   │     │           │
│  │  └──────────┘   └──────────┘   └──────────┘     │           │
│  └────────────────────────────────────────────────┘           │
│                            ↓           ↓                       │
│                    m_axis_tcc   m_axis_image                    │
└─────────────────────────────────────────────────────────────────┘
                             ↓              ↓
                      Host 端 calcKernels   空中像输出
                             ↓
                       SOCS Kernels
```

**接口更换示例（后期 PCIe 升级）**：
```tcl
# Vivado Block Design 更换：
# - JTAG-to-AXI Master + AXI BRAM Controller (删除)
# + PCIe DMA Engine + AXI Interconnect (新增)

# HLS 代码无需修改
# host/adapter 代码需修改（新增 PCIe DMA 驱动）
```

---

### 📁 推荐目录结构（严格化）

```
source/TCC_HLS/                    ← 与现有工程结构一致
├── hls/                            ← HLS 源码（生成 IP 核的核心）
│   ├── src/
│   │   ├── tcc_top.cpp             ← dataflow 顶层（AXI-Lite + AXI-Stream 接口）
│   │   ├── tcc_top.h
│   │   ├── calc_tcc.cpp            ← pupil + TCC 计算
│   │   ├── calc_tcc.h
│   │   ├── fft_2d.cpp              ← 2D FFT（hls::fft 封装）
│   │   ├── fft_2d.h
│   │   ├── calc_image.cpp          ← 频域卷积
│   │   ├── calc_image.h
│   │   ├── axi_bram_adapter.cpp    ← BRAM ↔ Stream 适配器（JTAG 用）
│   │   └── data_types.h            ← float/hls::complex<float> 类型定义
│   ├── tb/
│   │   ├── tb_tcc.cpp              ← C 仿真测试平台
│   │   ├── tb_full_system.cpp      ← 端到端 CoSim 测试
│   │   └── golden/                 ← 小尺寸验证数据
│   │       ├── source_33x33.bin    ← srcSize=33（而非 101，减少仿真时间）
│   │       ├── mask_128x128.bin    ← Lx=Ly=128
│   │       └── expected_tcc.bin    ← CPU 参考输出
│   ├── script/
│   │   ├── config/
│   │   │   └ hls_config.cfg        ← HLS 配置文件（器件、时钟）
│   │   ├── run_csynth.tcl          ← C 综合 TCL
│   │   └ run_cosim.tcl              ← CoSim TCL
│   │   └ export_ip.tcl              ← IP 导出 TCL
│   │   └ verify_bram.tcl            ← 板级验证 TCL（JTAG-to-AXI）
│   └ directives.tcl                 ← pragma 指令集中管理
│   └── hls_litho_proj/              ← HLS 工作目录（由 vitis-run 生成）
│       └ solution1/
│       │   ├── impl/
│       │   │    export/rtl/     ← 导出的 RTL 包
│       │   ├── csim/
│       │   └ cosim/
│       └ reports/\                  ← 资源/时序报告
├── host/                           ← 当前阶段暂不实现（Phase 5 后期）
│   └── README_HOST.md              ← 说明后期 PCIe 升级路径
├── vivado_bd/                      ← Vivado Block Design 示例
│   ├── design.tcl                  ← BD 自动构建脚本
│   └ hls_ip_test.xpr               ← 测试工程
│   └ constraint.xdc               ← 约束文件
├── data/
│   ├── input/                      ← 复用 input/ 目录（config.json + *.bin）
│   └ output/                       ← 输出结果
├── Makefile                        ← 一键构建脚本
└── README.md
```

**⚠️ 关键约束**（便于后期更换接口）：
1. **目录统一**：`source/TCC_HLS/`，与现有工程结构一致
2. **TCL 脚本独立**：`script/` 子目录，便于后期替换 JTAG → PCIe 脚本
3. **HLS 工作目录**：`hls_litho_proj/` 由 vitis-run 自动生成
4. **golden 数据**：小尺寸（33×33, 128×128），降低仿真时间
5. **host/adapter**：当前仅保留 README，Phase 5 后期实现 JTAG/PCIe 驱动
6. **vivado_bd/**：存放 Vivado BD 示例，便于后期更换接口 IP 核

---

### 📝 完整任务清单（最终修正版）

#### Phase 0: 项目初始化
| #   | 任务                                                                            | 优先级 | 预计工时 | 验收标准                                                           | 状态 |
| --- | ------------------------------------------------------------------------------- | ------ | -------- | ------------------------------------------------------------------ | ---- |
| 0.1 | 创建目录结构 `source/TCC_HLS/`                                                  | P0     | 0.5d     | 目录结构与文档一致                                                 | ✅    |
| 0.2 | 定义数据类型 `data_types.h`（全用 `float` / `hls::complex<float>`，暂不用定点） | P0     | 0.5d     | 类型定义完成，编译通过                                             | ✅    |
| 0.3 | 编写 `hls_config.cfg`（参考参考文档）                                           | P0     | 0.5d     | 包含器件、时钟、源文件列表                                         | ✅    |
| 0.4 | 编写 `run_csynth.tcl` + `export_ip.tcl`                                         | P0     | 1d       | 可执行 vitis-run 命令                                              | ✅    |
| 0.5 | 准备 golden 数据（用 CPU 参考代码生成）                                         | P0     | 0.5d     | source_33x33.bin + mask_128x128.bin + expected_tcc.bin             | ✅    |
| 0.6 | 创建 `ALGORITHM_MATH.md`（算法数学公式文档）                                    | P0     | 0.5d     | 参考 `reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md`         | ✅    |
| 0.7 | 创建 `ARRAY_SIZE_CONSTRAINTS.md`（数组尺寸约束）                                | P0     | 0.5d     | 参考 `reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md` | ✅    |
| 0.8 | 创建 `Litho-TCC-HLS` 简化参考版本（固定数组+固定循环边界）                      | P1     | 1d       | hls_ref_types.h + hls_ref_pupil.cpp 完成（后续补充tcc/fft/image）  | ✅    |

**📌 HLS 配置文件模板（参考参考文档）**：
```cfg
# hls_config.cfg 核心内容
part = xcku3p-ffvb676-2-e
clock = 5ns
source_file = src/tcc_top.cpp
testbench_file = tb/tb_tcc.cpp
```

**📌 算法公式参考**（见 `reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md`）:
- Pupil Function: $P = \exp(i \cdot k \cdot dz \cdot \sqrt{1 - \rho^2 NA^2})$
- TCC 定义: $TCC(n_1, n_2) = \sum S \cdot P_1 \cdot P_2^*$
- 共轭对称性: $TCC(n_2, n_1) = TCC^*(n_1, n_2)$

**📌 数组尺寸约束**（见 `reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md`）:
- TCC 矩阵尺寸: tccSize = (2×Nx+1) × (2×Ny+1)
- 最小配置(CoSim): tccSize ≈ 16641 (Nx=Ny=64)
- BRAM容量: KU3P仅960KB，TCC需2.2GB → **必须使用DDR**

#### Phase 1: calcTCC 核心模块（最高优先级）
| #   | 任务                                                                | 优先级 | 预计工时 | 验收标准                   | 状态                          |
| --- | ------------------------------------------------------------------- | ------ | -------- | -------------------------- | ----------------------------- |
| 1.1 | 实现 `calc_tcc.cpp`（pupil 计算 + TCC 计算）                        | P0     | 3d       | 功能正确（CSIM 通过）      | ✅（C仿真通过，相对误差<1e-5） |
| 1.2 | 循环优化：添加 `#pragma HLS PIPELINE` + `UNROLL`                    | P0     | 2d       | Latency 报告显示优化效果   | ⬜                             |
| 1.3 | 三角函数替换：`hls::sin()` / `hls::cos()` / `hls::sqrt()`           | P0     | 1d       | 编译通过，无精度警告       | ✅                             |
| 1.4 | BRAM 分区：pupil 矩阵 `#pragma HLS ARRAY_PARTITION`                 | P0     | 1d       | 资源报告显示 BRAM 分布合理 | ⬜                             |
| 1.5 | **AXI-Lite 参数接口**：NA, lambda, defocus, Lx, Ly, Nx, Ny, srcSize | P0     | 1d       | 接口报告显示 AXI-Lite 端口 | ✅                             |
| 1.6 | C 仿真验证（对比 golden 数据）                                      | P0     | 1d       | 输出误差 < 1e-5            | ⬜（初始验证失败，需修正）     |
| 1.7 | C 综合 + CoSim（使用 `vitis-run` 命令）                             | P0     | 2d       | CoSim PASS                 | ⬜                             |

**📌 核心命令（必须使用 vitis-run，参考参考文档）**：
```bash
# C 仿真
vitis-run --mode hls --csim --config script/config/hls_config.cfg --work_dir hls_litho_proj

# C 综合
vitis-run --mode hls --tcl --input_file script/run_csynth.tcl --work_dir hls_litho_proj

# CoSim
vitis-run --mode hls --cosim --config script/config/hls_config.cfg --work_dir hls_litho_proj
```

#### Phase 2: FFT 模块
| #   | 任务                                                 | 优先级 | 预计工时 | 验收标准               | 状态 |
| --- | ---------------------------------------------------- | ------ | -------- | ---------------------- | ---- |
| 2.1 | 集成 `hls::fft`（参考 `interface_stream/fft_top.h`） | P0     | 2d       | 编译通过               | ⬜    |
| 2.2 | 实现 2D FFT：行 FFT + 转置 + 列 FFT                  | P0     | 2d       | 功能正确（CSIM）       | ⬜    |
| 2.3 | 实现 `fftshift`（myShift 函数移植）                  | P0     | 1d       | 输出对齐 CPU 参考      | ⬜    |
| 2.4 | AXI-Stream 接口封装                                  | P0     | 1d       | 接口报告显示 AXIS 端口 | ⬜    |
| 2.5 | r2c + c2r 独立测试                                   | P0     | 1d       | 单模块 CSIM PASS       | ⬜    |
| 2.6 | CoSim 验证                                           | P0     | 1.5d     | CoSim PASS             | ⬜    |

**📌 FFT 配置参考（`interface_stream/fft_top.h`）**：
```cpp
const int FFT_INPUT_WIDTH = 32;     // float 使用 32 位
const hls::ip_fft::arch FFT_ARCH = hls::ip_fft::pipelined_streaming_io;
```

#### Phase 3: calcImage 模块
| #   | 任务                              | 优先级 | 预计工时 | 验收标准         | 状态 |
| --- | --------------------------------- | ------ | -------- | ---------------- | ---- |
| 3.1 | 实现 `calc_image.cpp`（频域卷积） | P1     | 2d       | 功能正确（CSIM） | ⬜    |
| 3.2 | TCC 矩阵分块加载（避免全存储）    | P1     | 1.5d     | BRAM 使用量合理  | ⬜    |
| 3.3 | 循环流水线化                      | P1     | 1.5d     | Latency 优化明显 | ⬜    |
| 3.4 | CoSim 验证                        | P1     | 1d       | CoSim PASS       | ⬜    |

#### Phase 4: 系统集成 + 接口模块化
| #   | 任务                                         | 优先级 | 预计工时 | 验收标准                          | 状态 |
| --- | -------------------------------------------- | ------ | -------- | --------------------------------- | ---- |
| 4.1 | 实现 `tcc_top.cpp`（`#pragma HLS DATAFLOW`） | P0     | 2d       | 所有子模块串联                    | ⬜    |
| 4.2 | **AXI-Lite 寄存器映射**（见下方接口设计）    | P0     | 1d       | 所有参数可通过 AXI-Lite 配置      | ⬜    |
| 4.3 | **AXI4-Stream Slave** 输入（mask + source）  | P0     | 1d       | 接口报告显示 S_AXIS 端口          | ⬜    |
| 4.4 | **AXI4-Master** 存储接口（当前接 BRAM）      | P0     | 1.5d     | 接口报告显示 M_AXI 端口           | ⬜    |
| 4.5 | 实现 `axi_bram_adapter.cpp`                  | P0     | 1d       | JTAG 可加载数据                   | ⬜    |
| 4.6 | 端到端 CoSim（空中像 + TCC 输出）            | P0     | 3d       | 全流程 CoSim PASS                 | ⬜    |
| 4.7 | 导出 RTL 包（`vitis-run --package`）         | P0     | 1d       | 生成 `solution1/impl/export/rtl/` | ⬜    |

**📌 导出命令**：
```bash
vitis-run --mode hls --package --config script/config/hls_config.cfg --work_dir hls_litho_proj
```

#### Phase 5: Vivado 验证 + 板级测试
| #   | 任务                                                                          | 优先级 | 预计工时 | 验收标准                              | 状态 |
| --- | ----------------------------------------------------------------------------- | ------ | -------- | ------------------------------------- | ---- |
| 5.1 | Vivado Block Design：拖入 HLS IP + AXI BRAM Controller + JTAG-to-AXI          | P0     | 2d       | BD 连线正确，Generate Bitstream 成功  | ⬜    |
| 5.2 | 编写 `verify_bram.tcl`（参考参考文档 TCL 模板）                               | P0     | 1d       | 可执行 JTAG 加载 + 启动 IP            | ⬜    |
| 5.3 | 板级测试：JTAG 加载 golden 数据 → 计算 → 读空中像 + TCC → Host 端 calcKernels | P0     | 2d       | 空中像 + SOCS Kernels 与 CPU 参考一致 | ⬜    |
| 5.4 | 资源/时序分析                                                                 | P1     | 2d       | 时序收敛，资源利用率报告              | ⬜    |
| 5.5 | 定点优化（可选，第二阶段）                                                    | P2     | 3d       | 精度误差 < 0.1%                       | ⬜    |
| 5.6 | 编写 README + Vivado BD 示例                                                  | P1     | 1d       | 文档完整                              | ⬜    |

**📌 TCL 验证流程（参考参考文档）**：
```tcl
# 1. 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. Reset
reset_hw_axi [get_hw_axis hw_axi_1]

# 3. 写 AP_START 寄存器启动 IP
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn

# 4. 等待完成（轮询 ap_done）
# ...

# 5. 读结果
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 128 -type read
run_hw_axi_txn rd_txn
```

---

### 🔧 关键技术决策（严格化）

**1. 数据类型（Phase 0-4 全用浮点）**：
```cpp
// data_types.h
#include "hls_math.h"
#include <complex>
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;
// Phase 5 可选：切换为 ap_fixed<32,16>
```

**2. AXI-Lite 寄存器映射（满足要求2）**：
| 偏移地址 | 寄存器名 | 说明                                           |
| -------- | -------- | ---------------------------------------------- |
| 0x00     | AP_CTRL  | 控制寄存器（start=bit0, done=bit1, idle=bit2） |
| 0x10     | NA       | 数值孔径                                       |
| 0x14     | LAMBDA   | 波长 (nm)                                      |
| 0x18     | DEFOCUS  | 离焦量                                         |
| 0x20     | LX       | Mask 周期 X                                    |
| 0x24     | LY       | Mask 周期 Y                                    |
| 0x28     | SRC_SIZE | 光源网格尺寸                                   |
| 0x30     | NX       | TCC 维度 X                                     |
| 0x34     | NY       | TCC 维度 Y                                     |

**3. 接口设计（模块化，便于后期更换）**：
```cpp
void tcc_top(
    // ========== AXI-Lite 参数接口（所有参数可配置，不硬编码）==========
    volatile ap_uint<32>* ctrl_reg,       // NA, lambda, defocus, Lx, Ly, Nx, Ny, srcSize
    // ========== AXI4-Stream 数据输入（不依赖数据源）==========
    // 当前：JTAG → BRAM → Adapter → AXI-Stream
    // 后期：PCIe DMA → AXI-Stream（只需更换 Vivado BD 中的 Adapter）
    hls::stream<cmpxData_t>& s_axis_mask, // Mask 数据流
    hls::stream<cmpxData_t>& s_axis_src,  // Source 数据流
    // ========== AXI4-Master 存储（不依赖具体存储类型）==========
    // 当前：BRAM Controller
    // 后期：DDR (MIG)（只需更换 Vivado BD 连线）
    ap_uint<32>* m_axi_mem,               // TCC 矩阵存储
    // ========== AXI4-Stream 输出（不依赖具体输出方式）==========
    // 当前：JTAG 读 BRAM
    // 后期：PCIe DMA 接收
    hls::stream<cmpxData_t>& m_axis_image, // 空中像输出（Lx×Ly）
    hls::stream<cmpxData_t>& m_axis_tcc    // TCC 矩阵输出（供 Host 端计算 SOCS 核）
) {
    #pragma HLS INTERFACE s_axilite port=ctrl_reg bundle=CTRL
    #pragma HLS INTERFACE axis port=s_axis_mask
    #pragma HLS INTERFACE axis port=s_axis_src
    #pragma HLS INTERFACE m_axi port=m_axi_mem offset=direct
    #pragma HLS INTERFACE axis port=m_axis_image
    #pragma HLS INTERFACE axis port=m_axis_tcc
    #pragma HLS DATAFLOW
}
```

**📌 接口更换示例（后期 PCIe 升级）**：
```tcl
# Vivado Block Design 更换：
# 删除：JTAG-to-AXI Master + AXI BRAM Controller + axi_bram_adapter
# 新增：PCIe DMA Engine + AXI Interconnect

# HLS 代码无需修改
# host/adapter 代码需修改（新增 PCIe DMA 驱动）
```

**4. BIN 文件格式（参考 `input/BIN_Format_Specification.md`）**：
| 数据类型    | 格式                                                  |
| ----------- | ----------------------------------------------------- |
| Source      | float32, srcSize×srcSize, row-major                   |
| Mask        | float32, maskSizeX×maskSizeY, row-major               |
| 空中像输出  | float32, Lx×Ly, row-major                             |
| SOCS Kernel | float32, 实部/虚部分开存储 (krn_i_r.bin, krn_i_i.bin) |

**5. 后期升级路径**：
| 当前方案              | 后期升级                | 改动点                 |
| --------------------- | ----------------------- | ---------------------- |
| JTAG-to-AXI 加载 BRAM | PCIe DMA                | Vivado BD 替换 IP 核   |
| AXI4-Master → BRAM    | AXI4-Master → DDR (MIG) | Vivado BD 连线         |
| Host 端空             | Host XRT 程序           | 新增 host/xrt_test.cpp |

---

### ⚠️ 风险与注意事项

1. **TCC 矩阵尺寸**: tccSize = (2×Nx+1)×(2×Ny+1)，可能很大（如 81×81=6561），需分块处理
2. **精度问题**: 浮点 vs 定点需验证，Phase 5 可选定点优化
3. **2D FFT 转置**: 行-列分解需要转置，可能成为瓶颈，考虑使用 Line Buffer
4. **CoSim 时间**: 大尺寸仿真时间长，golden 数据建议用小尺寸（33×33, 128×128）
5. **特征值分解**: calcKernels 保留 CPU，不在 HLS 实现（调用 LAPACK zheevr_）

---

**最终回答您的4个问题**：
1. **是的**，最终得到 RTL 包，可直接导入 Block Design
2. **所有关键参数**通过 AXI-Lite 暴露，改参数只需写寄存器，无需重新综合
3. **JTAG 加载路径**已设计（`axi_bram_adapter` + `verify_bram.tcl`）
4. **接口模块化**：AXI-Stream 输入 + AXI-Master 存储，后期换 PCIe/DDR 只需改 Vivado BD 连线

---

**创建日期**: 2026-04-05
**状态**: Phase 0 已完成 (2026-04-06)
**下一阶段**: Phase 1 - calcTCC核心模块实现

