# FPGA-Litho 项目全局约束

## 项目概述

本项目将 `reference/CPP_reference/Litho-TCC` 重构为 Vitis HLS IP 核，最终目标是生成 RTL 包，可直接导入 Vivado Block Design。

---

## 参考资源

| 参考项 | 路径 | 用途 |
|--------|------|------|
| CPU 算法参考 | `reference/CPP_reference/Litho-TCC/` | calcTCC、FFT、calcImage、calcKernels 算法逻辑 |
| HLS FFT 参考 | `reference/vitis_hls_ftt的实现/interface_stream/` | hls::fft 集成模板 |
| 命令参考 | `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md` | vitis-run 命令、TCL验证流程 |
| TCL 脚本模板 | `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl` | JTAG-to-AXI 操作模板 |
| BIN 格式规范 | `input/BIN_Format_Specification.md` | 输入数据格式定义 |
| 配置参数 | `input/config/config.json` | 光学参数、尺寸参数定义 |

---

## 严格控制流程

### HLS 命令规范

**必须使用 `vitis-run`，禁止使用旧版 `vitis_hls` 或 `vivado_hls`**：

```bash
# C 仿真（必须指定 --csim 参数）
vitis-run --mode hls --csim --config script/config/hls_config.cfg --work_dir hls_litho_proj

# C 综合（推荐 TCL 驱动，必须指定 --tcl 参数）
vitis-run --mode hls --tcl --input_file script/run_csynth.tcl --work_dir hls_litho_proj

# CoSim（必须指定 --cosim 参数）
vitis-run --mode hls --cosim --config script/config/hls_config.cfg --work_dir hls_litho_proj

# IP 导出（必须指定 --package 参数）
vitis-run --mode hls --package --config script/config/hls_config.cfg --work_dir hls_litho_proj
```

**⚠️ 关键约束**：`--mode hls` 必须与操作类型参数配合使用（--csim/--cosim/--package/--tcl），否则会报错：
```
ERROR: Option --mode hls should be specified with one of the following options [--csim | --cosim | --impl | --package | --tcl | --itcl]
```

### TCL 验证流程（板级）

```tcl
# 1. 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. Reset JTAG-to-AXI
reset_hw_axi [get_hw_axis hw_axi_1]

# 3. 启动 HLS IP（写 AP_START）
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn

# 4. 等待完成（轮询 ap_done）

# 5. 读结果
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 128 -type read
run_hw_axi_txn rd_txn
```

---

## 数据类型规范

**Phase 0-4 全用浮点**（Phase 5 可选定点优化）：

```cpp
// data_types.h
#include "hls_math.h"
#include <complex>
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;
```

---

## BIN 文件格式

- **数据类型**: float32 (4 字节/元素)
- **存储顺序**: row-major
- **Source**: srcSize × srcSize
- **Mask**: maskSizeX × maskSizeY
- **Kernel**: (2×Nx+1) × (2×Ny+1)，实部/虚部分开存储

---

## 输出要求

### 最终输出（2项）

1. **空中像 (Aerial Image)**：
   - 格式：Lx × Ly float32 数组
   - 内容：光刻成像强度分布

2. **SOCS Kernels**：
   - 格式：nk 个复数核函数，每个 (2×Nx+1) × (2×Ny+1)
   - 存储：实部/虚部分开（`krn_i_r.bin`, `krn_i_i.bin`）
   - 附加：`kernel_info.txt`（尺寸、数量、特征值）

### 输出文件结构

```
output/
├── image.bin            ← 空中像（float32, Lx×Ly）
├── image.png            ← 空中像可视化
├── kernels/
│   ├── krn_0_r.bin      ← 核0实部
│   ├── krn_0_i.bin      ← 核0虚部
│   ├── krn_1_r.bin
│   ├── krn_1_i.bin
│   ├── ...
│   └── kernel_info.txt  ← 元数据
│   └── png/             ← 可视化 PNG
```

---

## 模块划分（当前重构范围：Phase 0-4）

| 模块 | 执行位置 | 说明 |
|------|----------|------|
| calcTCC | FPGA (HLS) | 四层循环，核心性能瓶颈 |
| FFT (r2c/c2r) | FPGA (HLS) | 替换 fftw3 → hls::fft |
| calcImage | FPGA (HLS) | 频域卷积 → 空中像输出 |
| **calcKernels** | **不重构** | Host 端从 TCC 矩阵计算 SOCS 核 |

**⚠️ 注意**：当前阶段只重构 FPGA 端 TCC 模块，calcKernels 保留 Host 端执行（LAPACK zheevr_）。

---

## 接口设计规范（模块化，便于后期更换）

| 接口类型 | 当前实现 | 后期升级路径 | 更改点 |
|----------|----------|--------------|--------|
| 参数配置 | AXI-Lite 寄存器 | JTAG/PCIe Host 写寄存器 | 只改 Vivado BD 连线 |
| 数据输入 | JTAG → BRAM → AXI-Stream | PCIe DMA → AXI-Stream | Vivado BD 替换 IP 核 |
| 存储接口 | AXI-Master → BRAM | AXI-Master → DDR (MIG) | Vivado BD 连线 |
| 数据输出 | JTAG 读 BRAM | PCIe DMA 接收 | Vivado BD 替换 IP 核 |

```cpp
void tcc_top(
    // AXI-Lite 控制接口（所有参数可配置，不硬编码）
    volatile ap_uint<32>* ctrl_reg,
    // AXI4-Stream 输入（不依赖数据源）
    hls::stream<cmpxData_t>& s_axis_mask,
    hls::stream<cmpxData_t>& s_axis_src,
    // AXI4-Master 存储（不依赖具体存储类型）
    ap_uint<32>* m_axi_mem,
    // AXI4-Stream 输出 1：空中像
    hls::stream<cmpxData_t>& m_axis_image,
    // AXI4-Stream 输出 2：TCC 矩阵（供 Host 端计算 SOCS 核）
    hls::stream<cmpxData_t>& m_axis_tcc
);
```

**关键约束**：
1. **HLS 代码不依赖数据来源**：所有输入/输出均通过 AXI-Stream
2. **参数通过 AXI-Lite 传入**：不硬编码任何参数
3. **后期更换接口**：只需修改 Vivado BD，HLS 代码无需修改

---

## CPU 参考代码测试方法

### 运行命令

```bash
# 方式1: 直接运行已编译的exe（推荐）
cd /root/project/FPGA-Litho
python run_reference_test.py

# 方式2: 重新编译后运行
cd reference/CPP_reference/Litho-TCC
make clean && make
cd /root/project/FPGA-Litho
python run_reference_test.py
```

### 编译流程说明

- **run_reference_test.py** 直接执行已编译的 `./klitho_tcc`（不重新编译）
- 代码修改后需手动执行 `make clean && make` 重新编译
- 编译时间约 2-3 秒，运行时间约 0.05 秒

### 验证输出关键指标

测试完成后输出以下关键验证信息：

```
VERIFICATION SUMMARY (For HLS C-Sim):
Key values to compare with HLS output:
  - TCC Mean (Real): <值>
  - TCC StdDev (Real): <值>
  - Image Mean: <值>
  - Image StdDev: <值>
  - Image Max: <值>
  - Image Min: <值>
Recommended tolerance: relative error < 1e-5
```

### 模块耗时分解

测试输出包含各模块的执行时间：

```
PERFORMANCE SUMMARY (Module Breakdown):
  [Module 1] Load Input:        <us> μs
  [Module 2] Prepare Source:    <us> μs
  [Module 3] FFT Mask:          <us> μs
  [Module 4] calcTCC:           <us> μs
  [Module 5] calcImage:         <us> μs
  [Module 6] Output Aerial:     <us> μs
  [Module 7] calcKernels:       <us> μs
  Total:                        <us> μs
```

### 统计验证方法（替代MD5/SHA256）

**⚠️ 不使用 MD5/SHA256 checksum**，原因：
- 浮点数计算存在平台差异（CPU vs FPGA）
- FFT 运算顺序差异导致精度差异
- x87 FPU 与 IEEE-754 硬件实现差异

**推荐验证方法**：
| 验证项 | 容差标准 | 说明 |
|--------|----------|------|
| TCC Mean | 相对误差 < 1e-5 | TCC 矩阵均值（实部） |
| TCC StdDev | 相对误差 < 1e-5 | TCC 矩阵标准差 |
| Image Mean | 相对误差 < 1e-5 | 空中像均值 |
| Image StdDev | 相对误差 < 1e-5 | 空中像标准差 |
| Image Max/Min | 相对误差 < 1e-5 | 空中像极值 |

### 中间数据样本

测试输出包含中心区域样本数据（避免边界效应）：

```
TCC Matrix Samples (first 5 diagonal elements):
  TCC[0,0]: <real> + j<imag>
  TCC[1,1]: <real> + j<imag>
  ...

FFT Mask Samples (center region):
  FFT[center]: <real> + j<imag>
  ...

Aerial Image Samples (center region):
  Image[center]: <value>
  ...
```

---

## 验收标准

| 验收项 | 标准 |
|--------|------|
| C 仿真 | 输出与 CPU 参考误差 < 1e-5 |
| CoSim | PASS |
| IP 导出 | 生成 `solution1/impl/export/rtl/` |
| 板级测试 | JTAG 加载 → 计算 → 输出与参考一致 |

---

## 风险注意事项

1. **TCC 矩阵尺寸**: tccSize = (2×Nx+1)×(2×Ny+1)，可能很大，需分块处理
2. **2D FFT 转置**: 行-列分解需要转置，考虑 Line Buffer 优化
3. **CoSim 时间**: 使用小尺寸 golden 数据（33×33, 128×128）
4. **特征值分解**: calcKernels 保留 CPU，不在 HLS 实现

---

## 算法数学公式参考

> **完整公式文档**: `reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md`

### Pupil Function 计算

$$
P(s_x, s_y, n_x, n_y) = \exp\left(i \cdot k \cdot dz \cdot \sqrt{1 - \rho^2 \cdot NA^2}\right)
$$

其中：
- $s_x, s_y$: 光源点坐标（归一化，范围 [-1, 1]）
- $n_x, n_y$: 空间频率坐标（整数）
- $
ho^2 = f_x^2 + f_y^2$: 归一化空间频率平方
- $f_x = \frac{n_x}{Lx_{norm}} + s_x$, $f_y = \frac{n_y}{Ly_{norm}} + s_y$

**HLS 替换**: `sqrt()` → `hls::sqrt()`, `cos()/sin()` → `hls::cos()/hls::sin()`

### TCC 矩阵定义

$$
TCC(n_1, n_2) = \sum_{p,q} S(p,q) \cdot P(p,q,n_1) \cdot P^*(p,q,n_2)
$$

**共轭对称性**: $TCC(n_2, n_1) = TCC^*(n_1, n_2)$ → 只计算上三角

### fftshift 操作

$$
\text{shift}(x) = (x + \text{offset}) \mod \text{size}
$$

**推荐**: Host端预处理，HLS接收已shift的数据

### FFT 复数重组（FT_r2c核心）

**⚠️ 重要**: FFTW3输出是压缩格式（只输出正频率），需重组为完整复数矩阵。

**共轭对称性公式**:
$$
F(-f_x, -f_y) = F^*(f_x, f_y)
$$

- Left Half (负频率): 通过Right Half的复共轭计算
- Right Half (正频率): FFTW3直接输出

**HLS推荐**: 使用hls::fft的完整输出模式，无需手动重组

### calcImage 频域卷积

$$
I(n_2) = \sum_{n_1} M(n_1) \cdot M^*(n_2 + n_1) \cdot TCC(n_1, n_2 + n_1)
$$

**循环边界（⚠️ 非对称）**:
- 外层: `ny2 ∈ [-2Ny, 2Ny]`, `nx2 ∈ [0, 2Nx]`（注意X起点）
- 内层: `ny1 ∈ [-Ny, Ny]`, `nx1 ∈ [-Nx, Nx]`

**边界条件**: `if (abs(nx2+nx1) <= Nx && abs(ny2+ny1) <= Ny)`

---

## 数组尺寸约束参考

> **完整约束文档**: `reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md`

### BRAM 容量约束

| 资源 | 数量 | 总容量 |
|------|------|--------|
| BRAM (KU3P) | 216 | 960KB |

**关键约束**: TCC矩阵最小配置需要 **2.2GB** → **必须使用DDR存储**

### 配置方案

| 配置 | Lx×Ly | srcSize | Nx×Ny | tccSize | 存储策略 |
|------|-------|---------|-------|---------|----------|
| **最小(CoSim)** | 128×128 | 33 | 64×64 | 16641 | DDR+分块 |
| **中等** | 256×256 | 51 | 128×128 | 66049 | DDR+分块 |
| **实际** | 2048×2048 | 101 | 1024×1024 | 4M | DDR+On-the-fly |

### HLS 固定数组示例

```cpp
// hls_ref_types.h（最小配置）
const int SRC_SIZE = 33;
const int LX = 128;
const int NX_MAX = 64;
const int TCC_SIZE_MAX = (2*NX_MAX+1) * (2*NY_MAX+1);  // 16641

// 输入数组（可放入BRAM）
float src[SRC_SIZE * SRC_SIZE];  // 4KB
float msk[LX * LY];              // 64KB

// TCC矩阵（必须使用AXI-Master）
hls::complex<float> *tcc;  // 指针，通过AXI-Master访问DDR
```
