# SOCS HLS 模块

**项目**: FPGA-Litho 光刻成像加速  
**模块功能**: SOCS (Sum of Coherent Sources) 算法的 HLS 硬件实现  
**目标器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)  
**开发工具**: Vitis HLS 2025.2

---

## 📁 目录结构

```
SOCS_HLS/
├── src/                    # HLS 源代码
│   ├── socs_hls.cpp        # SOCS 核心算法实现 (calc_socs_hls)
│   ├── socs_fft.cpp        # 2D FFT/IFFT 封装
│   ├── socs_fft.h          # FFT 配置与数据类型定义
│   ├── socs_simple.cpp     # 简化版实现 (用于快速验证)
│   └── socs_simple.h       # 简化版头文件
│
├── tb/                     # HLS 测试平台 (Testbench)
│   ├── tb_socs_hls.cpp     # SOCS 全功能验证测试
│   ├── tb_socs_simple.cpp  # 简化版验证测试
│   └── tb_fft_minimal.cpp  # FFT 最小化功能测试
│
├── script/                 # HLS 综合脚本
│   ├── config/             # HLS 配置文件
│   │   ├── hls_config_socs_full.cfg    # SOCS 全功能综合配置
│   │   ├── hls_config_socs_simple.cfg  # SOCS 简化版配置
│   │   ├── hls_config_fft.cfg          # FFT 组件独立配置
│   │   └── hls_config_socs.cfg         # SOCS 基础配置
│   ├── run_csynth_socs_full.tcl         # SOCS 全功能 TCL 综合脚本
│   └── run_csynth_fft.tcl               # FFT 组件 TCL 综合脚本
│
├── data/                   # Golden 验证数据
│   ├── mskf_r.bin          # Mask 频域实部 (512×512 float)
│   ├── mskf_i.bin          # Mask 频域虚部 (512×512 float)
│   ├── scales.bin          # 特征值权重 (10 float)
│   ├── tmpImgp_pad32.bin   # 期望输出 Golden (17×17 float)
│   ├── kernels/            # SOCS kernel 数据
│   └── *.md                # 数据说明文档
│
├── doc/                    # 技术文档
│   ├── 板级验证指南.md      # JTAG-to-AXI 硬件验证流程
│   ├── FFT_NaN调试总结.md   # FFT 数值问题调试记录
│   ├── HLS验证进展总结.md   # 验证进度追踪
│   ├── HLS验证流程完整报告.md # C仿真→综合→CoSim完整报告
│   ├── IP端口连线指南.md    # Vivado Block Design 连线说明
│   ├── SOCS任务清单.md      # 开发任务分解
│   └── README.md           # 文档索引
│
├── validation/         # 板级验证（链接到 validation/board/）
│   ├── scripts/            # TCL/Python 验证脚本
│   └── RESET_FIX_GUIDE.md  # 复位问题修复指南
│
├── vivado_bd/              # Vivado Block Design 相关
│
├── logs/                   # HLS 综合日志
│
├── versions/               # 版本历史记录
│
├── socs_full_comp/         # SOCS 全功能综合输出 (v13+)
├── socs_full_comp_ku5p/    # Kintex UltraScale+ 专用综合输出
├── socs_simple_comp/       # SOCS 简化版综合输出
├── socs_simple_csim/       # 简化版 C 仿真输出
├── socs_simple_synth/      # 简化版 RTL 综合输出
│
├── .Xil/                   # Vitis HLS 工程缓存 (自动生成)
├── .gitignore              # Git 忽略配置
└── AddressSegments.csv     # AXI 地址段映射表
```

---

## 🔧 核心功能

### calc_socs_hls 算法

SOCS (Sum of Coherent Sources) 是一种光刻成像快速计算方法，通过分解光源为若干相干源，利用 FFT 加速成像计算。

**算法流程**:
1. 从 mask 频谱提取 9×9 区域
2. 与 SOCS kernel 点乘
3. 嵌入 32×32 padded 数组
4. 执行 32×32 2D IFFT
5. 提取有效 17×17 输出
6. 累加强度: $I = \sum_k \lambda_k \cdot (re^2 + im^2)$

**当前配置**:
- Nx = 4, Ny = 4 (频域采样)
- IFFT 尺寸 = 32×32 (zero-padded)
- 输出尺寸 = 17×17
- Kernel 数量 = 10

---

## 🚀 HLS 开发流程

### 推荐命令 (在 SOCS_HLS 目录下执行)

```bash
# 方式 1: v++ 一站式综合
v++ -c --mode hls --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp

# 方式 2: vitis-run + TCL
vitis-run --mode hls --tcl script/run_csynth_socs_full.tcl

# C 仿真
vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg --work_dir hls/socs_full_csim

# Co-Simulation
vitis-run --mode hls --cosim --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp

# IP 导出
vitis-run --mode hls --package --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp
```

### 性能指标 (v13)

| 指标           | 数值           |
| -------------- | -------------- |
| Estimated Fmax | 273.97 MHz     |
| Total Latency  | 201,833 cycles |
| BRAM           | 76 (10%)       |
| DSP            | 29 (2%)        |
| FF             | 18,120 (5%)    |
| LUT            | 22,906 (14%)   |

---

## 📊 IP 接口

calc_socs_hls IP 核包含以下接口:

| 接口类型        | 名称            | 用途                          |
| --------------- | --------------- | ----------------------------- |
| AXI4-Lite Slave | s_axi_control   | 控制寄存器 (ap_start/ap_done) |
| AXI4-Lite Slave | s_axi_control_r | 参数地址寄存器                |
| AXI4-Master     | m_axi_gmem[0-5] | DDR 内存访问 (6通道)          |
| Clock           | ap_clk          | 主时钟                        |
| Reset           | ap_rst_n        | 低电平复位                    |
| Interrupt       | interrupt       | 完成中断                      |

详细连线说明见 [`doc/IP端口连线指南.md`](./doc/IP端口连线指南.md)。

---

## 📖 相关文档

- **全局约束**: `../.github/copilot-instructions.md`
- **Golden 数据生成**: `../../validation/golden/run_verification.py`
- **配置参数**: `../../input/config/config.json`
- **文档索引**: [`doc/README.md`](./doc/README.md)

---

## 📅 更新记录

- **2026-04-19**: 目录结构整理，添加 README
- **2026-04-09**: v13 IP 导出成功
- **2026-04-07**: FFT 配置修复，验证流程完成