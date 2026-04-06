# SOCS HLS 重构 TODO

## 项目目标

将 `reference/CPP_reference/Litho-SOCS/klitho_socs.cpp` 中的 **calcSOCS** 函数重构为 Vitis HLS IP 核，最终生成 RTL 包可直接导入 Block Design。

---

## 算法分析（原始代码分解）

### calcSOCS 核心流程

```
输入: mskf[Lx×Ly] (已FFT), krns[nk×krnSize], scales[nk]
参数: Lx, Ly, Nx, Ny, nk

1. 循环nk个核 (k=0..nk-1):
   a. 频域点乘: bwdDataIn = krns[k] * mskf (中心区域)
      - 范围: x∈[-Nx,Nx], y∈[-Ny,Ny]
      - 偏移: difX=2Nx, difY=2Ny
   
   b. 2D IFFT: bwdDataOut = IFFT2D(bwdDataIn)
      - 尺寸: sizeX=4Nx+1, sizeY=4Ny+1
   
   c. 空域累加: tmpImg += scales[k] × |bwdDataOut|²

2. fftshift: tmpImgp = myShift(tmpImg, shiftType=true)

3. 傅里叶插值 (FI):
   a. R2C FFT: complexDataNxy = FFT(tmpImgp)
   b. Zero-padding: 扩展到 Lx×Ly 频域
   c. C2R IFFT: realDataLxy = IFFT(complexDataLxy)
   d. fftshift: image = myShift(realDataLxy)

输出: image[Lx×Ly] 空中像
```

---

## 阶段 0：项目基础设施搭建（第1周）

### 任务列表

- [ ] **创建目录结构**
  - `source/SOCS_HLS/src/` - HLS源代码
  - `source/SOCS_HLS/tb/` - Testbench
  - `source/SOCS_HLS/data/` - 测试数据（golden输入/输出）
  - `source/SOCS_HLS/script/` - TCL脚本
  - `source/SOCS_HLS/hls/` - HLS工作目录

- [ ] **配置环境与器件**
  - 目标器件: `xcku3p-ffvb676-2-e`
  - 时钟频率: 200 MHz (5ns)
  - Vitis HLS 版本确认

- [ ] **准备测试数据**
  - 从 `verification/` 目录提取 golden 数据：
    - `mskf_r.bin`, `mskf_i.bin` (mask频域)
    - `scales.bin` (特征值)
    - `kernels/krn_*_r/i.bin` (SOCS核)
    - `image.bin` (期望输出)
  - 配置参数: Lx=512, Ly=512, Nx=16, Ny=16, nk=16

- [ ] **编写基础 TCL 脚本**
  - `script/run_csynth.tcl` - C综合脚本
  - `script/run_cosim.tcl` - CoSim验证脚本
  - `script/run_package.tcl` - IP导出脚本

---

## 阶段 1：基础模块实现（第2周）

### 1.1 数据类型定义

**文件**: `src/data_types.h`

```cpp
#include <complex>
#include "ap_fixed.h"

// 全浮点方案（推荐）
typedef float data_t;
typedef std::complex<data_t> cmpxData_t;

// 定点方案（可选，资源优化）
// typedef ap_fixed<32,16> data_t;
// typedef std::complex<data_t> cmpxData_t;

// 常量约束
const int MAX_NK = 16;           // 最大核数量
const int MAX_NXY = 16;          // 最大Nx/Ny
const int MAX_IMG_SIZE = 512;    // 最大Lx/Ly
const int MAX_CONV_SIZE = 65;    // 4*16+1
```

- [ ] 定义数据类型结构体
- [ ] 定义常量约束（支持参数化尺寸）
- [ ] 定义接口类型（AXI-Lite, AXI-MM, AXI-Stream）

### 1.2 HLS FFT Wrapper

**文件**: `src/fft_wrapper.h`

参考 `reference/vitis_hls_fft的实现/interface_stream/fft_top.h`：

- [ ] **配置 FFT 参数**
  ```cpp
  // 临时图像IFFT配置 (sizeX×sizeY, 如17×17或65×65)
  const int FFT_LEN_TMP = 65;    // 最大变换长度
  const int FFT_NFFT_MAX_TMP = 6; // log2(65)
  
  // 傅里叶插值FFT配置 (Lx×Ly, 如512×512)
  const int FFT_LEN_FINAL = 512;
  const int FFT_NFFT_MAX_FINAL = 9;
  ```

- [ ] **实现 2D IFFT 函数**
  ```cpp
  void fft_2d_ifft(
      cmpxData_t* input,   // sizeX×sizeY频域
      cmpxData_t* output,  // sizeX×sizeY空域
      int sizeX, int sizeY
  );
  ```
  
  **关键挑战**: HLS不支持直接2D FFT，需实现：
  - Row-wise FFT (对每行做1D FFT)
  - Matrix Transpose
  - Column-wise FFT
  
  **参考方案**:
  - 使用 `hls::fft` IP库
  - Line Buffer实现transpose
  - DATAFLOW优化

- [ ] **实现 R2C FFT 函数**（傅里叶插值第一步）
  ```cpp
  void fft_r2c_2d(
      data_t* input_real,      // sizeX×sizeY实数
      cmpxData_t* output_comp, // sizeX×sizeY复数
      int sizeX, int sizeY
  );
  ```

- [ ] **实现 C2R IFFT 函数**（傅里叶插值第三步）
  ```cpp
  void fft_c2r_2d(
      cmpxData_t* input_comp,  // Lx×Ly复数
      data_t* output_real,     // Lx×Ly实数
      int sizeX, int sizeY
  );
  ```

### 1.3 工具函数模块

**文件**: `src/utils_hls.h`

- [ ] **实现 fftshift 函数**
  ```cpp
  template<typename T>
  void myShift(
      T* in, T* out,
      int sizeX, int sizeY,
      bool shiftTypeX, bool shiftTypeY
  );
  ```
  
  **HLS约束**: 需完全展开循环，使用BRAM存储

- [ ] **实现频域点乘函数**
  ```cpp
  void kernel_multiply(
      cmpxData_t* mskf,        // mask频域 (Lx×Ly中心区域)
      cmpxData_t* krn,         // SOCS核 (2Nx+1)×(2Ny+1)
      cmpxData_t* out,         // 输出 (sizeX×sizeY)
      int Nx, int Ny,
      int Lxh, int Lyh,
      int difX, int difY
  );
  ```

- [ ] **实现加权累加函数**
  ```cpp
  void accumulate_intensity(
      cmpxData_t* ifft_out,    // IFFT输出
      data_t* accum_image,     // 累加图像
      data_t scale,            // 特征值权重
      int sizeX, int sizeY
  );
  ```
  
  **公式**: `accum += scale × (re² + im²)`

- [ ] **实现频域 zero-padding**
  ```cpp
  void zero_padding_freq(
      cmpxData_t* in,          // sizeX×sizeY频域
      cmpxData_t* out,         // Lx×Ly频域
      int sizeX, int sizeY,
      int Lx, int Ly
  );
  ```

---

## 阶段 2：核心算法实现（第3周）

### 2.1 calcSOCS 主流程

**文件**: `src/calc_socs.cpp`

- [ ] **实现主函数框架**
  ```cpp
  void calcSOCS(
      // AXI-MM 输入
      cmpxData_t* mskf,        // mask频域 (Lx×Ly)
      cmpxData_t* krns,        // SOCS核数组 (nk × krnSize)
      data_t* scales,          // 特征值 (nk)
      
      // AXI-Stream 输出
      hls::stream<data_t>& image_stream,  // 空中像
      
      // AXI-Lite 参数
      int Lx, int Ly, int Nx, int Ny, int nk
  );
  ```

- [ ] **nk 循环并行优化**
  
  **方案 A**: 循环展开（UNROLL）
  ```cpp
  #pragma HLS UNROLL factor=4
  for (int k = 0; k < nk; k++) {
      // 核处理流程
  }
  ```
  
  **方案 B**: DATAFLOW 并行
  ```cpp
  #pragma HLS DATAFLOW
  for (int k = 0; k < nk; k++) {
      kernel_multiply(...);
      fft_2d_ifft(...);
      accumulate_intensity(...);
  }
  ```
  
  **推荐**: 先实现串行版本验证正确性，再优化并行

- [ ] **傅里叶插值流程**
  ```cpp
  // Step 1: R2C FFT
  fft_r2c_2d(tmpImgp, complexDataNxy, sizeX, sizeY);
  
  // Step 2: Zero-padding
  zero_padding_freq(complexDataNxy, complexDataLxy, sizeX, sizeY, Lx, Ly);
  
  // Step 3: C2R IFFT
  fft_c2r_2d(complexDataLxy, image, Lx, Ly);
  
  // Step 4: fftshift
  myShift(image, image_shifted, Lx, Ly, false, false);
  ```

### 2.2 Top-level 接口

**文件**: `src/socs_top.cpp`

- [ ] **AXI-Lite 控制接口**
  ```cpp
  volatile ap_uint<32>* ctrl_reg;  // [0]: Lx, [1]: Ly, [2]: Nx, [3]: Ny, [4]: nk, [5]: ap_start
  
  #pragma HLS INTERFACE s_axilite port=ctrl_reg bundle=ctrl
  ```

- [ ] **AXI4-Master 数据接口**
  ```cpp
  cmpxData_t* m_axi_mskf;   // mask频域
  cmpxData_t* m_axi_krns;   // SOCS核
  data_t* m_axi_scales;     // 特征值
  
  #pragma HLS INTERFACE m_axi port=m_axi_mskf depth=512*512
  #pragma HLS INTERFACE m_axi port=m_axi_krns depth=16*65*65
  ```

- [ ] **AXI4-Stream 输出**
  ```cpp
  hls::stream<data_t>& m_axis_image;
  
  #pragma HLS INTERFACE axis port=m_axis_image
  ```

- [ ] **完整 top 函数**
  ```cpp
  void socs_top(
      volatile ap_uint<32>* ctrl_reg,
      cmpxData_t* m_axi_mskf,
      cmpxData_t* m_axi_krns,
      data_t* m_axi_scales,
      hls::stream<data_t>& m_axis_image
  );
  ```

---

## 阶段 3：Testbench 与验证（第4周）

### 3.1 C Simulation Testbench

**文件**: `tb/socs_tb.cpp`

- [ ] **数据加载函数**
  ```cpp
  void load_test_data(
      cmpxData_t* mskf,
      cmpxData_t* krns,
      data_t* scales,
      data_t* golden_image
  );
  ```
  
  从 `verification/` 目录读取 golden 数据

- [ ] **误差计算函数**
  ```cpp
  bool verify_output(
      data_t* hls_output,
      data_t* golden_output,
      int Lx, int Ly,
      float tolerance = 1e-5
  );
  ```

- [ ] **完整 testbench 流程**
  ```cpp
  int main() {
      // 1. 加载测试数据
      // 2. 调用 socs_top
      // 3. 对比输出误差
      // 4. 输出验证结果
      return error_count;
  }
  ```

### 3.2 Co-Simulation Testbench

- [ ] **适配 Vitis HLS CoSim 格式**
  - 输入数据从文件加载
  - 输出数据写入文件供对比
  - 无交互式输出（如 cout）

- [ ] **时钟与接口配置**
  ```cpp
  #pragma HLS top
  #pragma HLS protocol
  ```

---

## 阶段 4：优化与综合（第5周）

### 4.1 性能优化策略

- [ ] **Pipeline 优化**
  - 对nk循环内各函数pipeline
  - 目标II=1

- [ ] **存储优化**
  - BRAM分区: `ARRAY_PARTITION cyclic factor=4`
  - 临时数组放入BRAM
  - 大数组（mskf）通过AXI-MM访问

- [ ] **数据流优化**
  - `DATAFLOW` pragma减少中间存储
  - Stream传递中间结果

- [ ] **资源估算**
  - DSP利用率: FFT主要消耗
  - BRAM利用率: <5% (当前尺寸)
  - 目标: 512×512配置下 <100k DSP

### 4.2 综合配置

**文件**: `script/run_csynth.tcl`

```tcl
open_project -reset socs_hls_proj
add_files src/socs_top.cpp
add_files src/calc_socs.cpp
add_files src/fft_wrapper.h
add_files src/utils_hls.h
add_files tb/socs_tb.cpp -tb
set_top socs_top

open_solution -reset solution1
set_part xcku3p-ffvb676-2-e
create_clock -period 5ns

# 配置参数
config_flow -target vivado
config_compile -unsafe_math_operations

# 综合指令
csim_design
csynth_design
cosim_design -trace
export_design -format rtl -output ../output/SOCS_RTL
```

- [ ] 编写完整 TCL 脚本
- [ ] 配置器件与时钟
- [ ] 配置综合优化参数

---

## 阶段 5：IP 集成与验证（第6周）

### 5.1 IP 导出

- [ ] **执行 C 综合**
  ```bash
  cd source/SOCS_HLS
  vitis-run --mode hls --tcl script/run_csynth.tcl
  ```

- [ ] **验证 C仿真结果**
  - 误差 < 1e-5（相对误差）
  - 输出统计: mean, max, min对比

- [ ] **执行 CoSim**
  ```bash
  vitis-run --mode hls --cosim --tcl script/run_cosim.tcl
  ```

- [ ] **导出 IP**
  ```bash
  vitis-run --mode hls --package --tcl script/run_package.tcl
  ```

### 5.2 Vivado 集成验证

- [ ] **导入 IP 到 Vivado**
  - 将 `output/SOCS_RTL/` 添加到 Vivado IP库
  - 更新 IP Catalog

- [ ] **Block Design 集成**
  - 创建 AXI-Interconnect
  - 连接 AXI-Lite 控制接口
  - 连接 AXI-Master 数据接口
  - 连接 AXI-Stream 输出

- [ ] **生成比特流**
  - Synthesis
  - Implementation
  - Generate Bitstream

### 5.3 板级测试（可选）

参考 `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md`：

- [ ] **JTAG-to-AXI 测试 TCL**
  ```tcl
  # 连接硬件
  connect_hw_server
  open_hw_target
  
  # 写入参数到 AXI-Lite
  create_hw_axi_txn write_params [get_hw_axis hw_axi_1] \
    -address 0x40000000 -data {0x00000200 0x00000200 ...} -len 6 -type write
  
  # 写入数据到 AXI-MM
  # 启动 IP
  # 等待 ap_done
  # 读结果
  ```

---

## 关键风险与应对

### 风险 1: 2D FFT 实现复杂

**应对策略**:
- 参考 Vitis DSP Library 的 2D FFT 示例
- 分解为 Row-FFT + Transpose + Col-FFT
- 使用 Line Buffer优化transpose

### 风险 2: FFT 尺寸匹配

**应对策略**:
- 临时图像IFFT: sizeX×sizeY (如17×17或65×65)
- 傅里叶插值: sizeX×sizeY → Lx×Ly
- 需实现动态FFT配置或分段实现

### 风险 3: nk 循环并行

**应对策略**:
- 先实现串行版本验证正确性
- 逐步增加 UNROLL factor
- 考虑资源约束（DSP数量）

### 风险 4: 数值精度

**应对策略**:
- 全浮点方案降低精度风险
- 验证时使用相对误差 < 1e-5
- CoSim验证RTL精度

---

## 验收标准

| 验收项 | 标准 |
|--------|------|
| C仿真 | 输出与CPU参考误差 < 1e-5 |
| CoSim | PASS |
| IP导出 | 生成 `output/SOCS_RTL/` |
| Vivado集成 | Block Design成功连接 |
| 性能目标 | Latency < 100k cycles (@200MHz) |

---

## 进度追踪

| 阶段 | 预计时间 | 状态 |
|------|----------|------|
| 阶段0：基础设施 | 第1周 | 待启动 |
| 阶段1：基础模块 | 第2周 | 待启动 |
| 阶段2：核心算法 | 第3周 | 待启动 |
| 阶段3：Testbench | 第4周 | 待启动 |
| 阶段4：优化综合 | 第5周 | 待启动 |
| 阶段5：IP集成 | 第6周 | 待启动 |

---

## 参考资源

| 参考项 | 路径 | 用途 |
|--------|------|------|
| CPU算法参考 | `reference/CPP_reference/Litho-SOCS/` | calcSOCS 算法逻辑 |
| HLS FFT参考 | `reference/vitis_hls_fft的实现/interface_stream/` | hls::fft 集成模板 |
| 命令参考 | `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow.md` | vitis-run 命令 |
| TCL模板 | `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl` | JTAG测试模板 |
| Golden数据 | `verification/` | 输入/输出验证数据 |
| 配置参数 | `input/config/config.json` | 参数定义 |

---

**创建日期**: 2026-04-07  
**最后更新**: 2026-04-07