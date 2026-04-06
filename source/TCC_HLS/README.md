# TCC HLS重构项目

将CPU端的TCC（传输交叉相关）计算模块重构为Vitis HLS IP核，用于光刻仿真加速。

## 项目概述

**当前阶段**: Phase 0 - 项目初始化  
**目标**: 为后续Phase 1-4（calcTCC、FFT、calcImage、系统集成）建立基础框架

**Phase 0-4范围**:
- ✅ Phase 0: 项目初始化（已完成）
- ⬜ Phase 1: calcTCC核心模块（Pupil计算 + TCC矩阵累加）
- ⬜ Phase 2: FFT模块（hls::fft集成）
- ⬜ Phase 3: calcImage模块（频域卷积）
- ⬜ Phase 4: 系统集成（tcc_top + AXI接口）
- ⬜ Phase 5: Vivado验证 + 板级测试（后期）

## 目录结构

```
source/TCC_HLS/
├── hls/                          # HLS核心代码
│   ├── src/                      # 源文件
│   │   ├── data_types.h          # ✅ 数据类型定义
│   │   ├── tcc_top.cpp           # ⬜ Phase 4添加
│   │   ├── calc_tcc.cpp          # ⬜ Phase 1添加
│   │   ├── calc_image.cpp        # ⬜ Phase 3添加
│   │   └── fft_2d.cpp            # ⬜ Phase 2添加
│   ├── tb/                       # 测试平台
│   │   ├── tb_tcc.cpp            # ✅ 测试骨架
│   │   └── golden/               # ✅ Golden数据
│   │       ├── image_expected_512x512.bin
│   │       ├── source_101x101.bin
│   │       ├── mask_256x256.bin
│   │       ├── kernels/
│   │       └── golden_stats.txt
│   └── script/                   # 构建脚本
│       ├── config/
│       │   └── hls_config.cfg    # ✅ HLS配置
│       ├── run_csim.tcl          # ✅ C仿真脚本
│       ├── run_csynth.tcl        # ✅ C综合脚本
│       ├── run_cosim.tcl         # ✅ CoSim脚本
│       └── export_ip.tcl         # ✅ IP导出脚本
│   └── hls_litho_proj/           # 📁 HLS工作目录（vitis-run生成）
├── host/                         # Host适配器（Phase 5+）
│   └ README_HOST.md              # ✅ PCIe升级路径说明
├── vivado_bd/                    # Vivado Block Design示例（Phase 5+）
├── data/                         # 数据目录
│   ├── input/                    # 输入数据（symlink → ../../input/）
│   └ output/                     # 输出结果
├── Makefile                      # ✅ 构建自动化
└ README.md                       # ✅ 本文档
```

## 构建流程

### 前置条件

1. **Vitis HLS 2025.2+**（支持`vitis-run`命令）
2. **器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)
3. **时钟**: 5ns (200 MHz)

### 构建命令

```bash
# 进入项目目录
cd source/TCC_HLS

# 1. C仿真（验证算法正确性）
make csim

# 2. C综合（生成RTL）
make csynth

# 3. CoSim（RTL级验证）
make cosim

# 4. 导出IP（生成Vivado IP核）
make package

# 5. 全流程（顺序执行）
make all

# 6. 清理工作目录
make clean
```

### 详细命令（手动执行）

```bash
# C仿真
vitis-run --mode hls --csim \
  --config hls/script/config/hls_config.cfg \
  --work_dir hls_litho_proj

# C综合（推荐TCL驱动）
vitis-run --mode hls --tcl \
  --input_file hls/script/run_csynth.tcl \
  --work_dir hls_litho_proj

# CoSim
vitis-run --mode hls --cosim \
  --config hls/script/config/hls_config.cfg \
  --work_dir hls_litho_proj

# IP导出
vitis-run --mode hls --package \
  --config hls/script/config/hls_config.cfg \
  --work_dir hls_litho_proj
```

## 测试流程

### Phase 0验证（当前）

```bash
# 1. 验证目录结构
ls -R hls/

# 2. 验证数据类型编译
g++ -std=c++11 -c hls/src/data_types.h -o /dev/null

# 3. 验证testbench编译
g++ -std=c++11 -I hls/src hls/tb/tb_tcc.cpp -o tb_tcc
./tb_tcc  # 应输出统计信息

# 4. 验证golden数据存在
ls -lh hls/tb/golden/
```

### Phase 1-4验证（后续）

```bash
# C仿真验证（对比golden数据）
make csim
# 查看报告：hls_litho_proj/solution1/csim/report/tcc_top_csim.log

# CoSim验证（RTL级）
make cosim
# 查看报告：hls_litho_proj/solution1/cosim/report/
```

## 输出产物

### Phase 0输出（当前）

- ✅ 目录结构建立
- ✅ 数据类型定义（`data_types.h`）
- ✅ HLS配置文件（`hls_config.cfg`）
- ✅ TCL构建脚本（4个）
- ✅ Golden测试数据
- ✅ Testbench骨架（`tb_tcc.cpp`）

### Phase 1-4输出（后续）

- **C仿真报告**: `solution1/csim/report/`
- **综合报告**: `solution1/syn/report/`
  - Latency分析
  - 资源利用率（BRAM, DSP, FF, LUT）
- **CoSim报告**: `solution1/cosim/report/`
- **IP核**: `solution1/impl/export/ip/*.zip`

### Phase 5输出（后期）

- Vivado Block Design工程
- 板级验证脚本（`verify_bram.tcl`）
- 空中像输出（`image.bin`）
- SOCS Kernels（由Host端计算）

## 关键技术决策

### 1. 数据类型（Phase 0-4）

```cpp
typedef float data_t;                    // 浮点（Phase 0-4）
typedef std::complex<data_t> cmpxData_t;

// Phase 5可选：定点优化
// typedef ap_fixed<32,16> data_t;
```

### 2. 接口设计（模块化）

| 接口类型 | 当前实现 | 后期升级 | 改动点 |
|----------|----------|----------|--------|
| 参数配置 | AXI-Lite寄存器 | PCIe/PCIe | Vivado BD连线 |
| 数据输入 | JTAG→BRAM→AXI-Stream | PCIe DMA→AXI-Stream | Vivado BD替换IP |
| 存储 | AXI-Master→BRAM | AXI-Master→DDR(MIG) | Vivado BD连线 |
| 输出 | JTAG读BRAM | PCIe DMA接收 | Vivado BD替换IP |

**关键约束**:
- ✅ HLS代码不依赖数据来源（全部通过AXI-Stream）
- ✅ 参数通过AXI-Lite传入（不硬编码）
- ✅ 后期换PCIe只需改Vivado BD，HLS无需修改

### 3. 数组尺寸策略

| 配置 | Lx×Ly | srcSize | Nx×Ny | tccSize | 存储策略 |
|------|-------|---------|-------|---------|----------|
| **最小(CoSim)** | 128×128 | 33 | 64×64 | 16641 | DDR+分块 |
| **中等(功能验证)** | 256×256 | 51 | 128×128 | 66049 | DDR+分块 |
| **实际** | 2048×2048 | 101 | 1024×1024 | 4M | DDR+On-the-fly |

**⚠️ 重要**: TCC矩阵最小配置需2.2GB → **必须使用DDR存储**

## 算法参考

### 核心公式

1. **Pupil函数**:
   $$P(s_x, s_y, n_x, n_y) = \exp\left(i \cdot k \cdot dz \cdot \sqrt{1 - \rho^2 \cdot NA^2}\right)$$

2. **TCC定义**:
   $$TCC(n_1, n_2) = \sum_{p,q} S(p,q) \cdot P(p,q,n_1) \cdot P^*(p,q,n_2)$$

3. **频域卷积**:
   $$I(n_2) = \sum_{n_1} M(n_1) \cdot M^*(n_2 + n_1) \cdot TCC(n_1, n_2 + n_1)$$

### 参考资源

| 参考项 | 路径 |
|--------|------|
| CPU算法 | `reference/CPP_reference/Litho-TCC/klitho_tcc.cpp` |
| 算法公式 | `reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md` |
| 尺寸约束 | `reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md` |
| FFT集成模板 | `reference/vitis_hls_ftt的实现/interface_stream/` |
| HLS参考类型 | `reference/CPP_reference/Litho-TCC-HLS/source/hls_ref_types.h` |
| 配置参数 | `input/config/config.json` |
| 全局约束 | `.github/copilot-instructions.md` |

## 常见问题

### Q1: 为什么不用MD5/SHA256校验golden数据？

**A**: 浮点计算存在平台差异（CPU vs FPGA）:
- FFT运算顺序差异
- x87 FPU vs IEEE-754硬件差异
- 推荐使用统计验证（相对误差 < 1e-5）

### Q2: Phase 0为何使用512×512配置？

**A**: 当前使用CPU参考输出作为golden数据，该输出为512×512配置。
Phase 1-4时会生成128×128（CoSim配置）的小尺寸golden数据以降低仿真时间。

### Q3: calcKernels为何不在HLS中实现？

**A**: 特征值分解（LAPACK zheevr_）复杂度高，不适合HLS实现。
由Host端从TCC矩阵计算SOCS核，符合CPU-GPU异构计算模式。

## 联系方式

- 项目负责人: [待补充]
- 创建日期: 2026-04-06
- 最后更新: 2026-04-06

## License

[待补充]