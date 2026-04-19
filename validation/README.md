# FPGA-Litho 验证与测试目录

本目录整合了 **Golden 数据生成** 与 **板级硬件验证** 两套流程，提供完整的 HLS IP 验证体系。

---

## 目录职能划分

```
validation/
├── README.md                    # 本文档（总体说明）
├── golden/                      # 【Golden 数据生成】算法验证与参考数据
│   ├── README.md                # 详细使用指南
│   ├── run_verification.py      # Python 验证入口
│   ├── debug_nan_source.py      # NaN 调试工具
│   └── src/                     # C++ 验证程序源码
│       ├── litho.cpp            # TCC + SOCS 实现
│       ├── Makefile
│       └── common/              # 公共库 (file_io, mask, source)
└── board/                       # 【板级硬件验证】FPGA 硬件测试
    ├── README.md                # 详细使用指南
    ├── common/                  # 公共工具脚本
    ├── jtag/                    # ⭐ JTAG-to-AXI 验证（当前可用）
    │   ├── README.md            # JTAG 验证操作手册
    │   ├── socs_hls_validation.tcl
    │   ├── bin_to_tcl_converter.py
    │   └── data/                # 验证数据文件
    └── pcie/                    # 🔲 PCIe DMA 验证（待开发）
```

---

## 两套流程职能对比

| 流程                | 目录      | 目的                           | 输出                                       | 适用阶段           |
| ------------------- | --------- | ------------------------------ | ------------------------------------------ | ------------------ |
| **Golden 数据生成** | `golden/` | 验证算法正确性，生成参考数据   | BIN 格式参考数据（`output/verification/`） | HLS C 仿真、Co-Sim |
| **板级验证**        | `board/`  | 验证 HLS IP 在真实硬件上的行为 | 硬件运行结果（通过 JTAG 读取）             | FPGA 板级测试      |

---

## 快速使用指南

### 1. Golden 数据生成

**用途**：生成 TCC/SOCS 算法的参考输出，用于 HLS C 仿真和 Co-Simulation 验证。

```bash
# 项目根目录快捷入口
python verify.py --clean           # 清理后重新验证
python verify.py --debug           # 调试模式（输出中间结果）
python verify.py --compile-only    # 仅编译 C++ 程序

# 或直接运行
cd validation/golden/src
make clean && make
./litho <config.json> <output_dir>
```

**输出位置**：`output/verification/` (BIN 格式，IEEE 754 float32)

**关键文件**：
- `mskf_r.bin`, `mskf_i.bin` — Mask 频域数据
- `scales.bin`, `kernels/*.bin` — SOCS 核数据
- `aerial_image_tcc_direct.bin` — TCC 直接成像（理论标准）
- `aerial_image_socs_kernel.bin` — SOCS 核重建成像

---

### 2. 板级硬件验证

**用途**：在 FPGA 板卡上运行 HLS IP，验证硬件行为与 Golden 数据一致性。

**当前可用方式**：JTAG-to-AXI

```bash
# 1. 将 Golden 数据转换为 TCL 格式
cd validation/board/jtag
python bin_to_tcl_converter.py

# 2. 在 Vivado Hardware Manager 中执行验证
source socs_hls_validation.tcl
```

**详细操作**：参见 [`board/jtag/README.md`](board/jtag/README.md)

---

## 数据流向示意

```
                    ┌─────────────────────────────────┐
                    │       input/config.json         │
                    │    (光学参数、尺寸参数)          │
                    └──────────────────┬──────────────┘
                                       │
                                       ▼
┌──────────────────────┐      ┌──────────────────────┐
│  validation/golden/  │      │   mask 频域数据      │
│  run_verification.py │ ──▶  │   (mskf_r/i.bin)     │
│  (Python + C++)      │      │   SOCS kernels       │
└──────────────────────┘      └──────────────────────┘
                    │                    │
                    ▼                    ▼
        ┌────────────────────┐   ┌──────────────────────┐
        │  output/           │   │  validation/board/   │
        │  validation/golden/│   │  jtag/data/          │
        │  (Golden BIN)      │──▶│  (TCL 格式)          │
        └────────────────────┘   └──────────────────────┘
                                       │
                                       ▼
                            ┌──────────────────────┐
                            │  FPGA Hardware       │
                            │  (JTAG-to-AXI)       │
                            └──────────────────────┘
```

---

## 相关文档

| 文档         | 路径                                                                                          | 内容                               |
| ------------ | --------------------------------------------------------------------------------------------- | ---------------------------------- |
| HLS 验证流程 | [`source/SOCS_HLS/doc/HLS验证流程完整报告.md`](../source/SOCS_HLS/doc/HLS验证流程完整报告.md) | C Sim → C Synth → Co-Sim → Package |
| IP 端口连线  | [`source/SOCS_HLS/doc/IP端口连线指南.md`](../source/SOCS_HLS/doc/IP端口连线指南.md)           | Vivado Block Design 连线           |
| 板级验证指南 | [`source/SOCS_HLS/doc/板级验证指南.md`](../source/SOCS_HLS/doc/板级验证指南.md)               | 硬件测试完整流程                   |
| BIN 格式规范 | [`input/BIN_Format_Specification.md`](../input/BIN_Format_Specification.md)                   | 输入数据格式定义                   |