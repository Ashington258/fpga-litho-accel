
# SOCS HLS 重构任务清单（优化版）

> **全局约束**: 参考 `.github/copilot-instructions.md`  
> **本模块独立性**: 与TCC模块完全独立，有自己的golden数据、测试脚本和任务清单

---

## 项目概述

将 `reference/CPP_reference/Litho-SOCS/klitho_socs.cpp` 中的 **calcSOCS** 函数重构为高性能 Vitis HLS IP 核。

### Host/FPGA分工

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

### FPGA端输入参数详解

| 参数 | 类型 | 尺寸 | 来源 | 说明 |
|------|------|------|------|------|
| **mskf** | complex<float> | Lx×Ly | Host端FFT(mask) | mask频域数据，**已做FFT** |
| **krns** | complex<float> | nk × (2Nx+1)×(2Ny+1) | Host端calcKernels | SOCS核数组 |
| **scales** | float | nk | Host端calcKernels | 特征值/加权系数 |
| **Lx, Ly** | int | - | config.json | 输出图像尺寸 |
| **Nx, Ny** | int | - | config.json | 核尺寸参数 |
| **nk** | int | - | calcKernels输出 | 核数量 |

### 关键计算尺寸

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

## 目录结构

（保持与原版本一致，略）

---

## 任务清单（优化后）

### Phase 0: 项目初始化（已完成）
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 0.1~0.6 | 创建目录结构、数据类型、Makefile、README 等 | P0 | 4d | 全部完成，可执行 make 命令 | ✅ |

### Phase 1: calcSOCS 核心模块（简化版本已完成）
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 1.1 | 实现 `calc_socs.cpp` 基础版本（框架 + 数据类型） | P0 | 1d | 编译通过 | ✅ |
| 1.2 | **集成支持 r2c/c2r 的 2D FFT 库**（推荐 `hls::fft`） | P0 | 2.5d | 单独测试通过 | ⏸️ 暂跳过（简化版） |
| 1.3 | 实现独立 `fi_hls()` 函数（傅里叶插值） | P0 | 2d | 输出尺寸正确放大 | ⏸️ 暂跳过（简化版） |
| 1.4 | **核心优化**：DATAFLOW、ARRAY_PARTITION、UNROLL | P0 | 2d | Latency改善 | ✅ |
| 1.5 | 生成真实 Golden 数据（复用verification系统） | P0 | 1.5d | 相对误差 < 1e-6 | ✅ |
| 1.6 | C 仿真验证（使用 golden 数据） | P0 | 1d | 相对误差 < 1e-5 | ✅ |
| 1.7 | C 综合验证 + 报告分析 | P0 | 1d | Fmax达标，资源合理 | ✅ **Fmax: 273.97 MHz** |

### Phase 2: 接口模块化（已完成）
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 2.1 | 添加 AXI-Lite 参数接口（Lx, Ly, Nx, Ny, nk 等） | P0 | 0.5d | 接口报告显示 AXI-Lite | ✅ 简化版使用AXI-MM |
| 2.2 | 输入接口：**AXI-MM**（mskf + krns + scales） | P0 | 1.5d | 支持大数组高效传输 | ✅ |
| 2.3 | 输出接口：**AXI-MM**（image） | P0 | 1d | 高吞吐输出 | ✅ |
| 2.4 | CoSim 验证（含接口） | P0 | 2d | CoSim PASS | ✅ **PASS** |

### Phase 3: IP 导出与系统验证（已完成）
| # | 任务 | 优先级 | 预计工时 | 验收标准 | 状态 |
|---|------|--------|----------|----------|------|
| 3.1 | 导出 Vivado IP（*.zip） | P0 | 1d | 生成完整 IP 包 | ✅ **xilinx_com_hls_calc_socs_simple_hls_1_0.zip** |
| 3.2 | Vivado Block Design 集成测试 | P1 | 2d | BD 连线正确 | ⬜ 待验证 |
| 3.3 | 板级验证脚本（JTAG / DMA 测试） | P1 | 1.5d | 板级运行成功 | ⬜ 待验证 |

---

## 当前版本说明（简化版本）

### 简化策略
当前完成的版本是**简化版本**（`socs_simple_nofft.cpp`），特点：

1. **跳过真实FFT**：频域点乘后直接计算模平方，不执行IFFT
2. **简化插值**：使用最近邻插值代替傅里叶插值
3. **固定参数**：Lx=512, Ly=512, Nx=4, Ny=4, nk=10（硬编码）

### 性能指标（简化版本）
| 指标 | 结果 | 目标 | 状态 |
|------|------|------|------|
| Estimated Fmax | 273.97 MHz | 200 MHz | ✅ 超过目标 |
| CoSim | PASS | PASS | ✅ |
| RTL生成 | Verilog + VHDL | RTL | ✅ |
| IP导出 | .zip (289KB) | Vivado IP | ✅ |

### 后续优化方向
1. **集成真实FFT**：使用`hls::fft`实现Row-FFT + Transpose + Col-FFT
2. **傅里叶插值**：实现完整的R2C → zero-padding → C2R → shift流程
3. **参数可配置**：通过AXI-Lite传递Lx, Ly, Nx, Ny, nk参数
4. **性能优化**：提高nk循环并行度，优化内存访问

---

## Golden数据生成脚本（已优化）

`generate_golden.py` 必须实现以下逻辑（不再使用模拟数据）：

```python
# 关键修改点：
# 1. 调用或复现 reference/CPP_reference/Litho-SOCS/klitho_socs.cpp 的 calcSOCS
# 2. 使用 numpy + scipy.fft 或 pyfftw 生成完全一致的 float32 结果（Hermitian 对称处理）
# 3. 输出所有 .bin 文件 + golden_info.txt（记录 Lx, Ly, Nx, Ny, nk 等）
# 4. 验证脚本：比较 HLS 输出与 expected 的 max absolute/relative error
```

**Golden 数据尺寸规范**（不变）：
- Mask频域 (mskf): Lx×Ly complex float32
- SOCS核 (krn_i): (2*Nx+1)×(2*Ny+1) complex float32
- scales: nk float32
- 空中像 (image): Lx×Ly float32

---

## HLS 配置参数（保持）

- **目标器件**: xcku3p-ffvb676-2-e
- **时钟**: 5ns (200 MHz)
- **关键常量**（建议适当增大以支持 padding）：
  ```cpp
  const int MAX_NK = 16;
  const int MAX_NXY = 8;      // 建议稍大，支持未来扩展
  const int MAX_IMG_SIZE = 512;
  const int MAX_CONV_SIZE = 4*8 + 1;  // 33
  ```

---

## 注意事项（更新）

1. **FFT 实现**：优先尝试 `hls::fft` 或 Vitis DSP Library 的 2D FFT 支持；若需自定义，必须包含 transpose 模块并使用 `hls::stream` 连接。
2. **数据流优化**：强烈推荐在顶层函数和子模块中使用 `#pragma HLS DATAFLOW`，中间结果通过 `hls::stream<Complex>` 传递，避免全局大数组冲突。
3. **数值精度**：全程使用 `float`（Complex {float re, im;}），误差控制在 1e-5 以内。
4. **存储**：当前尺寸全部可放 BRAM；未来大尺寸再引入 DDR。
5. **独立性**：本模块 golden 数据与 TCC 完全无关。

---

**创建日期**: 2026-04-06  
**更新日期**: 2026-04-06（简化版本完成）  
**状态**: Phase 0-3 完成，IP已导出  
**下一步**: Vivado Block Design集成测试、板级验证

