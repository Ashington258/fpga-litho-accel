# FPGA-Litho SOCS 模块全局约束

## 项目概述

本项目将 `reference/CPP_reference/Litho-SOCS/klitho_socs.cpp` 中的 **calcSOCS** 函数重构为 Vitis HLS IP 核，最终目标是得到标准 Vivado IP（`.zip`），可直接拖入 Vivado Block Design。

---

## 参考资源

| 参考项 | 路径 | 用途 |
|--------|------|------|
| CPU 算法参考 | `reference/CPP_reference/Litho-SOCS/` | calcSOCS 算法逻辑、FFT调用、傅里叶插值 |
| HLS FFT 参考 | `reference/vitis_hls_ftt的实现/interface_stream/` | hls::fft 集成模板 |
| 命令参考 | `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md` | vitis-run 命令、TCL验证流程 |
| TCL 脚本模板 | `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl` | JTAG-to-AXI 操作模板 |
| BIN 格式规范 | `input/BIN_Format_Specification.md` | 输入数据格式定义 |
| 配置参数 | `input/config/config.json` | 光学参数、尺寸参数定义 |
| 任务清单 | `source/SOCS_HLS/SOCS_TODO.md` | 详细任务分解与进度追踪 |

---

## 严格控制流程

### HLS 命令规范（已验证的正确方案）

**两种可行的C综合方式（均已验证）**：

#### 方案1：v++ 命令方式（推荐用于FFT组件）
```bash
# C仿真 + C综合（一站式）
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls --config script/config/hls_config_fft.cfg --work_dir fft_2d_forward_32

# 结果：Estimated Fmax 273.97 MHz，Latency 19,407 cycles
```

#### 方案2：vitis-run TCL方式（适合完整项目流程）
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --tcl script/run_csynth_fft.tcl

# 结果：同方案1，耗时约39秒
```

#### C仿真单独运行
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_fft.cfg --work_dir hls/fft_2d_forward_32
```

#### CoSim验证（需先完成C综合）
```bash
vitis-run --mode hls --cosim --config script/config/hls_config_fft.cfg --work_dir fft_2d_forward_32
```

#### IP导出
```bash
vitis-run --mode hls --package --config script/config/hls_config_fft.cfg --work_dir fft_2d_forward_32
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

# 5. 读结果（空中像）
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 128 -type read
run_hw_axi_txn rd_txn
```

---

## Host/FPGA 分工

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              数据流与分工                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        Host 端职责                                   │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ 1. 读取 config.json (NA, lambda, defocus, Lx, Ly, Nx, Ny等)        │   │
│  │ 2. 读取/生成 mask 数据 (Lx×Ly float32)                              │   │
│  │ 3. 【calcKernels】从TCC矩阵计算SOCS核 (LAPACK特征值分解)            │   │
│  │    → 输出: krns[nk][krnSizeX×krnSizeY], scales[nk]                 │   │
│  │ 4. 对mask做FFT得到mask频域数据 mskf[Lx×Ly] (complex float32)        │   │
│  │ 5. 准备数据传输给FPGA                                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                        │
│                    ┌───────────────────────────────────┐                    │
│                    │  AXI-Stream / AXI-MM 传输         │                    │
│                    └───────────────────────────────────┘                    │
│                                    ↓                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        FPGA 端职责 (本模块)                          │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │ 输入:                                                               │   │
│  │   - mskf: mask频域数据 (Lx×Ly complex float32) ← Host已FFT         │   │
│  │   - krns: SOCS核数组 (nk个, 每个(2Nx+1)×(2Ny+1) complex float32)   │   │
│  │   - scales: 特征值/加权系数 (nk个 float32)                          │   │
│  │   - 参数: Lx, Ly, Nx, Ny, nk                                        │   │
│  │                                                                     │   │
│  │ 处理流程 (calcSOCS):                                                │   │
│  │   1. 循环nk个核:                                                    │   │
│  │      a. 核与mask频域点乘 (中心区域 y∈[-Ny,Ny], x∈[-Nx,Nx])         │   │
│  │      b. IFFT变换到空域 (sizeX=4Nx+1, sizeY=4Ny+1)                  │   │
│  │      c. |z|² 加权累加 (scales[k] × (re² + im²))                    │   │
│  │   2. fftshift (myShift)                                             │   │
│  │   3. 傅里叶插值放大 (sizeX×sizeY → Lx×Ly)                           │   │
│  │                                                                     │   │
│  │ 输出:                                                               │   │
│  │   - image: 空中像 (Lx×Ly float32)                                   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## FPGA 端输入参数详解

| 参数 | 类型 | 尺寸 | 来源 | 说明 |
|------|------|------|------|------|
| **mskf** | complex<float> | Lx×Ly | Host端FFT(mask) | mask频域数据，**已做FFT** |
| **krns** | complex<float> | nk × (2Nx+1)×(2Ny+1) | Host端calcKernels | SOCS核数组 |
| **scales** | float | nk | Host端calcKernels | 特征值/加权系数 |
| **Lx, Ly** | int | - | config.json | 输出图像尺寸 |
| **Nx, Ny** | int | - | config.json | 核尺寸参数 |
| **nk** | int | - | calcKernels输出 | 核数量 |

---

## 关键计算尺寸

```cpp
// 从参数计算得到的尺寸
int krnSizeX = 2 * Nx + 1;     // 核X尺寸 (如 Nx=4 → 9)
int krnSizeY = 2 * Ny + 1;     // 核Y尺寸 (如 Ny=4 → 9)
int sizeX = 4 * Nx + 1;        // 临时图像X尺寸 (如 Nx=4 → 17)
int sizeY = 4 * Ny + 1;        // 临时图像Y尺寸 (如 Ny=4 → 17)
int Lxh = Lx / 2;              // Mask中心偏移X
int Lyh = Ly / 2;              // Mask中心偏移Y
int difX = sizeX - krnSizeX;   // X填充偏移 = 2*Nx
int difY = sizeY - krnSizeY;   // Y填充偏移 = 2*Ny
```

---

## 数据类型规范

**全用浮点**：

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
- **Mask频域**: Lx×Ly complex (实部/虚部交替存储)
- **SOCS核**: nk × (2Nx+1)×(2Ny+1) complex (实部/虚部交替存储)
- **scales**: nk float32
- **空中像**: Lx×Ly float32

---

## 输出要求

### 最终输出

**空中像 (Aerial Image)**：
- 格式：Lx×Ly float32 数组
- 内容：光刻成像强度分布

### 输出文件结构

```
output/
├── image.bin            ← 空中像（float32, Lx×Ly）
├── image.png            ← 空中像可视化
```

---

## 接口设计规范

```cpp
void socs_top(
    // AXI-Lite 控制接口（所有参数可配置）
    volatile ap_uint<32>* ctrl_reg,  // [0]: Lx, [1]: Ly, [2]: Nx, [3]: Ny, [4]: nk, [5]: ap_start
    
    // AXI4-Master 输入（大数组高效传输）
    cmpxData_t* m_axi_mskf,          // mask频域数据 (Lx×Ly)
    cmpxData_t* m_axi_krns,          // SOCS核数组 (nk × krnSize)
    data_t* m_axi_scales,            // 特征值数组 (nk)
    
    // AXI4-Stream 输出（高吞吐）
    hls::stream<data_t>& m_axis_image  // 空中像输出 (Lx×Ly)
);
```

**关键约束**：
1. **Host端预处理**：mask已做FFT，SOCS核已计算完成
2. **参数通过 AXI-Lite 传入**：不硬编码任何参数
3. **大数组通过 AXI-MM**：支持 burst 传输
4. **输出通过 AXI-Stream**：高吞吐、无阻塞

---

## Golden 数据验证方法

### 统一验证入口

**使用 `verification/` 目录的独立验证系统**：

```bash
cd /root/project/FPGA-Litho
python verify.py                      # 默认验证
python verify.py --clean              # 清理重新验证
python verification/run_verification.py --debug  # 调试模式
```

### 验证输出文件（SOCS HLS Golden 输入）

| 文件 | 尺寸 | 说明 | SOCS HLS用途 |
|------|------|------|--------------|
| `mskf_r.bin` | Lx×Ly float32 | Mask频域实部 | **AXI-MM输入** |
| `mskf_i.bin` | Lx×Ly float32 | Mask频域虚部 | **AXI-MM输入** |
| `scales.bin` | nk float32 | 特征值数组 | **AXI-MM输入** |
| `kernels/krn_*_r/i.bin` | tccSize float32 | SOCS核数据 | **AXI-MM输入** |
| `kernels/kernel_info.txt` | 文本 | 核数量、尺寸、特征值 | 参数配置 |
| `image.bin` | Lx×Ly float32 | SOCS空中像输出 | **HLS输出对比** |
| `image_tcc.bin` | Lx×Ly float32 | TCC空中像输出 | 交叉验证 |

### 验证关键指标

```
VERIFICATION SUMMARY:
Key values to compare with HLS output:
  - Image Mean: <值>
  - Image StdDev: <值>
  - Image Max: <值>
  - Image Min: <值>
Recommended tolerance: relative error < 1e-5
```

---

## 验收标准

| 验收项 | 标准 |
|--------|------|
| C 仿真 | 输出与 CPU 参考误差 < 1e-5 |
| CoSim | PASS |
| IP 导出 | 生成 `solution1/impl/export/ip/*.zip` |
| 板级测试 | JTAG 加载 → 计算 → 输出与 golden 一致 |
| 性能目标 | 512×512、nk=16 配置下，Latency < 100k cycles (< 0.5ms @200MHz) |

---

## 算法数学公式参考

### calcSOCS 核心算法

**核与mask频域点乘**：

$$
M_k(n_x, n_y) = mskf(Lxh + n_x, Lyh + n_y) \cdot krn_k(n_x, n_y)
$$

其中：
- $n_x \in [-Nx, Nx], n_y \in [-Ny, Ny]$
- $Lxh = Lx/2, Lyh = Ly/2$（mask中心偏移）

**IFFT 变换**：

$$
I_k(x, y) = \text{IFFT}(M_k) \quad \text{尺寸: sizeX} \times \text{sizeY}
$$

其中 $\text{sizeX} = 4Nx+1, \text{sizeY} = 4Ny+1$。

**加权累加**：

$$
I_{sum}(x, y) = \sum_{k=0}^{nk-1} scales[k] \cdot |I_k(x, y)|^2
$$

**傅里叶插值（FI）**：

$$
I_{final} = \text{FI}(I_{sum}) \quad \text{尺寸: sizeX} \times \text{sizeY} \to Lx \times Ly
$$

FI 步骤：
1. R2C FFT（sizeX×sizeY）
2. 频域 zero-padding（扩展到 Lx×Ly）
3. C2R IFFT（Lx×Ly）
4. fftshift

---

## 数组尺寸约束

### BRAM 容量约束

| 资源 | 数量 | 总容量 |
|------|------|--------|
| BRAM (KU3P) | 216 | 7776KB (7.6MB) |

### 配置方案

| 配置 | Lx×Ly | Nx×Ny | nk | 临时尺寸 | 存储策略 |
|------|-------|-------|----|---------|---------|
| **最小(CoSim)** | 128×128 | 4×4 | 16 | 17×17 | BRAM (利用率<1%) |
| **中等** | 256×256 | 8×8 | 16 | 33×33 | BRAM (利用率<1%) |
| **实际** | 512×512 | 16×16 | 16 | 65×65 | BRAM (利用率1.5%) |

### HLS 固定数组示例

```cpp
// hls_config.h
const int MAX_NK = 16;
const int MAX_NXY = 16;           // 支持 Nx/Ny 最大16
const int MAX_IMG_SIZE = 512;     // Lx/Ly 最大512
const int MAX_CONV_SIZE = 4*16 + 1;  // 65

// 输入数组（通过AXI-MM传入）
cmpxData_t mskf[MAX_IMG_SIZE * MAX_IMG_SIZE];     // 最多2048KB complex (2MB)
cmpxData_t krns[MAX_NK * MAX_CONV_SIZE * MAX_CONV_SIZE];  // 约68KB
data_t scales[MAX_NK];                             // 64B

// 临时缓冲（可放入BRAM）
cmpxData_t tmp_conv[MAX_CONV_SIZE * MAX_CONV_SIZE];  // 17KB complex
data_t tmp_image[MAX_CONV_SIZE * MAX_CONV_SIZE];     // 8.5KB
```

---

## 风险注意事项

1. **2D FFT 实现**: HLS 不直接支持 2D FFT，需实现 row-FFT + transpose + col-FFT
2. **transpose 模块**: 需要高效的矩阵转置，考虑 Line Buffer 优化
3. **nk 循环并行**: 使用 DATAFLOW + UNROLL 实现 nk 个核的并行处理
4. **傅里叶插值**: 包含 R2C + zero-padding + C2R + fftshift，需完整实现
5. **数值精度**: 全程使用 float，误差控制在 1e-5 以内

---

## HLS 配置参数

```cpp
// 器件配置
const char* TARGET_DEVICE = "xcku3p-ffvb676-2-e";
const float CLOCK_PERIOD = 5.0;  // ns (200 MHz)

// 关键 pragma 示例
#pragma HLS DATAFLOW
#pragma HLS ARRAY_PARTITION variable=tmp_conv cyclic factor=4 dim=1
#pragma HLS UNROLL factor=4 for (int k = 0; k < nk; k++)
```

---

## 注意事项

1. **模块独立性**: 本模块与 TCC 模块完全独立，有自己的 golden 数据和测试脚本
2. **数据流优化**: 推荐使用 `#pragma HLS DATAFLOW`，中间结果通过 `hls::stream` 传递
3. **FFT 集成**: 优先尝试 `hls::fft` 或 Vitis DSP Library，若需自定义需包含 transpose
4. **存储策略**: 当前尺寸全部可放 BRAM，未来大尺寸可扩展 DDR
5. **Host端预处理**: mask已做FFT，核已计算，FPGA只做最终卷积+插值

---

**创建日期**: 2026-04-06  
**适用模块**: SOCS HLS  
**协同文件**: `source/SOCS_HLS/SOCS_TODO.md`  
**TCC 约束文件**: `.github/copilot-instructions-TCC.md`（已备份）
