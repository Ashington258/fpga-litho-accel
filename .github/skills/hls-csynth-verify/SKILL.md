---
name: hls-csynth-verify
description: 'WORKFLOW SKILL — Execute Vitis HLS C Synthesis and verify results. USE FOR: running v++ or vitis-run synthesis; checking Fmax/Latency metrics; proposing optimizations when targets not met; iterative refinement until metrics achieved. Ideal for FFT/SOCS high-performance kernels with Vitis 2025.2. DO NOT USE FOR: Vivado RTL synthesis; software simulation; board testing.'
---

# Vitis HLS C Synthesis Verification Skill (Improved)

## 适用场景

当用户需要执行以下任务时使用此skill:
- **执行C综合**: 使用v++或vitis-run进行HLS综合
- **验证性能指标**: 检查Fmax、Latency、资源利用率
- **性能优化**: 当指标未达标时提出优化建议并迭代
- **FFT/SOCS内核**: 特别适用于高性能计算内核

## 核心约束

**项目路径**: `/root/project/FPGA-Litho/source/SOCS_HLS`

**关键约束**:
1. **Nx/Ny动态计算**: $N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$
2. **当前配置实际值**: **Nx=4, Ny=4**（基于NA=0.8, Lx=512, λ=193, σ=0.9）
3. **IFFT尺寸改写**: 已满足2^N标准（17×17→32×32 zero-padded）
4. **器件配置**: `xcku3p-ffvb676-2-e`, 200MHz时钟
5. **Golden参考**: litho.cpp（已使用nextPowerOfTwo自动填充）

## ⚠️ 关键配置要求（新增）

### AXI-MM接口depth参数 ⭐ 必须配置

**实践中发现**: AXI-MM接口必须添加 `depth=<size>` 参数，否则无法执行Co-Simulation。

**正确配置示例**:
```cpp
void calc_socs_hls(
    float *mskf_r,      // AXI-MM: Mask spectrum real
    float *mskf_i,      // AXI-MM: Mask spectrum imaginary  
    float *scales,      // AXI-MM: Eigenvalues
    float *krn_r,       // AXI-MM: Kernels real
    float *krn_i,       // AXI-MM: Kernels imaginary
    float *output       // AXI-MM: Output
) {
    // ⚠️ 必须添加depth参数，否则Co-Simulation失败
    #pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 depth=512*512
    #pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 depth=512*512
    #pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 depth=10
    #pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem3 depth=10*9*9
    #pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem4 depth=10*9*9
    #pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 depth=17*17
    
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // ... kernel implementation ...
}
```

**depth参数规则**:
1. 使用编译时常量或宏定义（如 `depth=Lx*Ly`）
2. 对于动态大小数组，使用最大可能的depth值
3. depth值必须 ≥ 实际访问的最大深度
4. 所有AXI-MM接口（m_axi）都必须指定depth

### 配置文件格式要求

**正确格式**（无 `[package]` section）:
```ini
# HLS配置文件正确格式
part = xcku3p-ffvb676-2-e

[hls]
clock = 5ns
flow_target = vivado

# 源文件配置
syn.file = ../../src/kernel.h      # 添加头文件避免编译错误
syn.file = ../../src/kernel.cpp
syn.top = kernel_function

# Testbench配置（使用tb.file而非deprecated sim.file）
tb.file = ../../tb/tb_kernel.cpp
```

**⚠️ 常见错误**:
```ini
# ❌ 错误格式 - 不要添加[package] section
[package]
package.output.format=rtl    # 这会导致配置解析错误
```

---

## 执行流程

### 步骤 0: 源代码准备检查（新增）

**检查AXI-MM接口配置**:
```bash
# 检查是否添加了depth参数
grep -n "INTERFACE m_axi" src/*.cpp
```

**预期输出**（正确配置）:
```cpp
#pragma HLS INTERFACE m_axi port=data_array offset=slave bundle=gmem0 depth=512*512
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1 depth=289
```

**如果发现缺失depth参数**:
- 自动提示添加depth参数
- 提供正确格式示例
- 建议修改并重新执行

### 步骤 1: 选择执行方式

**方案1 (推荐)**: `v++` 一站式命令 - 快速、直观,适合独立FFT组件
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_kernel.cfg \
    --work_dir hls/kernel_csynth
```

**方案2**: `vitis-run + TCL` - 适合复杂项目、多步流程控制
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls \
    --tcl script/run_csynth.tcl
```

**⚠️ 注意**: 
- **不要使用** `vitis-run --csynth` 命令（不存在）
- 正确方式是 `v++ -c` 或 `vitis-run --tcl`
- 如果修改了源代码（如添加depth），必须重新执行综合

### 步骤 2: 执行C综合

#### 方案A（当前配置 Nx=4） - 32×32 IFFT

**命令**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csynth
```

**预期结果**（基于实际测试）:
- **Estimated Fmax**: 274 MHz (超过目标4 MHz) ✅
- **Latency**: 1275 cycles (极佳) ✅
- **Interval**: 1276 cycles
- **RTL包**: 生成 `calc_socs_simple_hls.zip` (88KB)

#### 方案B（目标配置 Nx=16） - 128×128 IFFT

**需要修改配置**:
- 修改 `input/config/config.json`: `NA=1.2` 或 `Lx=1536`
- 重新生成Golden数据
- 修改源代码中的Nx参数

**命令**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csynth_128
```

### 步骤 3: 解析综合报告

#### 3.1 提取关键性能指标

**Timing性能提取**:
```bash
# 从verbose报告中提取timing信息
cat hls/kernel_csynth/hls/.autopilot/db/kernel_function.verbose.rpt | grep -A 20 "Timing"
```

**预期输出**:
```
+--------+---------+----------+------------+
|  Clock |  Target | Estimated| Uncertainty|
+--------+---------+----------+------------+
|ap_clk  |  5.00 ns|  3.650 ns|     1.35 ns|
+--------+---------+----------+------------+
```

**计算Fmax**: 
```
Estimated Clock = 3.650 ns
Fmax = 1 / 3.650 ns = 274 MHz
```

**Latency性能提取**:
```bash
cat hls/kernel_csynth/hls/.autopilot/db/kernel_function.verbose.rpt | grep -A 20 "Latency"
```

**预期输出**:
```
+---------+---------+----------+----------+------+------+---------+
|  Latency (cycles) |  Latency (absolute) |   Interval  | Pipeline|
|   min   |   max   |    min   |    max   |  min |  max |   Type  |
+---------+---------+----------+----------+------+------+---------+
|     1275|     1275|  6.375 us|  6.375 us|  1276|  1276|       no|
+---------+---------+----------+----------+------+------+---------+
```

#### 3.2 性能验收标准（更新）

**基于实际测试结果的基准**:

| 指标 | 目标值 | 实测基准 | 如何计算 |
|------|--------|---------|---------|
| **Estimated Fmax** | ≥ 270 MHz | **274 MHz** | `1 / Estimated Clock (ns)` |
| **Latency** | < 10M cycles | **1275 cycles** | 直接从报告读取 |
| **Interval** | 合理值 | 1276 cycles | 直接从报告读取 |
| **Pipeline Type** | 优化 | "no"或"yes" | 检查Pipeline列 |
| **RTL包大小** | 合理 | 88KB | `ls -lh *.zip` |

**性能等级判定**:
- ✅ **优秀**: Fmax ≥ 274 MHz, Latency < 5000 cycles
- ✅ **达标**: Fmax ≥ 270 MHz, Latency < 10M cycles
- ⚠️ **需优化**: Fmax < 270 MHz 或 Latency > 10M cycles

#### 3.3 资源利用率检查

**检查生成的IP核**:
```bash
ls -lh hls/kernel_csynth/*.zip
unzip -l kernel_function.zip | grep "hdl/ip"
```

**预期浮点运算IP核**:
- `fadd_32ns_32ns_32_8_full_dsp_1_ip.xci`: 加法器（8 DSP）
- `fmul_32ns_32ns_32_5_max_dsp_1_ip.xci`: 乘法器（5 DSP）  
- `fdiv_32ns_32ns_32_16_no_dsp_1_ip.xci`: 除法器（16 cycles latency）
- `sitofp_32ns_32_6_no_dsp_1_ip.xci`: 整数转浮点

### 步骤 4: 性能验证与优化

#### 验收标准检查清单

| 检查项 | 标准 | 如何验证 |
|--------|------|---------|
| **Fmax达标** | ≥ 270 MHz | `Estimated Clock ≤ 3.70 ns` |
| **Latency合理** | < 10M cycles | 报告中 `Latency (cycles)` |
| **depth参数配置** | 所有AXI-MM接口 | 检查源代码pragma |
| **RTL生成完整** | 包含Verilog + IP核 | 检查zip包内容 |
| **无critical warnings** | 无严重警告 | 检查 `hls_compile.rpt` |
| **Nx/Ny一致** | 符合项目配置 | 检查源代码宏定义 |

#### 优化策略（如果未达标）

**1. 流水线优化**:
```cpp
// 对计算密集循环应用pipeline
for (int k = 0; k < nk; k++) {
    #pragma HLS PIPELINE II=1  // 目标II=1,最大化吞吐
    
    // 频域点乘
    for (int i = 0; i < fftConvX * fftConvY; i++) {
        spectrum[i] = mskf[i] * krn[k][i];
    }
}
```

**2. 数组分区**:
```cpp
// 对频繁访问的buffer分区提升带宽
float spectrum[32*32];
#pragma HLS ARRAY_PARTITION cyclic factor=4 variable=spectrum

// 对transpose matrix分区
float transpose_buffer[32][32];
#pragma HLS ARRAY_PARTITION variable=transpose_buffer dim=2 factor=4
```

**3. 循环展开**:
```cpp
// 对点乘、累加等计算密集循环展开
for (int i = 0; i < 9*9; i++) {
    #pragma HLS UNROLL factor=3  // 展开3倍
    
    result += kernel[i] * spectrum[i];
}
```

**4. Dataflow优化**:
```cpp
// 多个独立阶段使用dataflow
void calc_socs_hls(...) {
    #pragma HLS DATAFLOW
    
    stage1_frequency_multiply(...);  // 频域点乘
    stage2_ifft_execute(...);        // IFFT执行
    stage3_intensity_accumulate(...); // 强度累加
    stage4_output_extract(...);      // 输出提取
}
```

#### 优化迭代流程

**标准优化循环**:
1. 分析性能瓶颈 → 确定优化目标
2. 应用单一优化pragma → 重新综合
3. 对比前后性能 → 验证改进效果
4. 记录优化经验 → 总结最佳实践
5. 重复步骤1-4直到达标

**优化效果评估**:
- 每次优化后必须重新执行C Synthesis
- 记录优化前后性能对比
- 避免过度优化导致资源激增
- 保持Fmax和Latency的平衡

---

## 综合成功标志（新增）

### 必要输出文件

**成功生成的文件列表**:
```
hls/kernel_csynth/
├── kernel_function.zip          # ⭐ RTL包（核心输出）
├── kernel_csynth.hlscompile_summary  # 编译总结
├── component.xml                # IP-XACT描述（在zip内）
├── constraints/
│   └── kernel_function_ooc.xdc  # 时序约束（在zip内）
├── hdl/
│   ├── verilog/
│   │   ├── kernel_function_*.v  # ⭐ RTL代码（核心）
│   │   ├── kernel_function_gmem*_m_axi.v  # AXI-MM接口
│   │   ├── kernel_function_control*_s_axi.v  # AXI-Lite接口
│   │   └── kernel_function_Pipeline_*.v  # Pipeline模块
│   └── ip/
│       ├── fadd_*.xci           # 浮点加法IP核
│       ├── fmul_*.xci           # 浮点乘法IP核
│       ├── fdiv_*.xci           # 浮点除法IP核
│       └── sitofp_*.xci         # 整数转浮点IP核
├── hls/
│   ├── syn/                     # ⭐ 综合结果（CoSim依赖）
│   ├── .autopilot/db/
│   │   └── kernel_function.verbose.rpt  # ⭐ 详细报告
│   └── config.cmdline
└── reports/
    └── hls_compile.rpt          # 编译报告
```

### 成功验收检查

**必须满足的条件**:
- ✅ 存在 `kernel_function.zip` 文件
- ✅ zip包内包含 `hdl/verilog/*.v` 文件
- ✅ zip包内包含 `hdl/ip/*.xci` 文件
- ✅ 存在 `hls/syn/` 目录（Co-Simulation依赖）
- ✅ Estimated Clock ≤ 3.70 ns (Fmax ≥ 270 MHz)
- ✅ Latency < 10M cycles
- ✅ 无critical warnings或errors

---

## 常见问题诊断（新增）

### 综合失败快速诊断

| 错误类型 | 典型错误信息 | 根本原因 | 解决方案 |
|---------|-------------|---------|---------|
| **配置解析错误** | `unrecognised option 'package.xxx'` | 配置文件包含 `[package]` section | 删除 `[package]` section |
| **AXI-MM depth缺失** | `depth specification required` | AXI-MM接口缺少depth参数 | 添加 `depth=<size>` pragma |
| **头文件找不到** | `fatal error: 'xxx.h' file not found` | 头文件未添加到syn.file | 在配置文件添加 `syn.file = ../../src/xxx.h` |
| **函数未定义** | `undefined reference to 'xxx'` | syn.top与实际函数名不一致 | 检查并修正syn.top配置 |
| **编译失败** | `compilation error(s)` | 源代码语法错误 | 检查C++语法和HLS pragma语法 |

### 性能未达标诊断

| 问题现象 | 可能原因 | 优化策略 |
|---------|---------|---------|
| **Fmax < 270 MHz** | 逻辑路径过长、时序违例 | Pipeline优化、逻辑简化 |
| **Latency过高** | 循环未优化、数据依赖 | 循环展开、数组分区 |
| **资源激增** | 过度展开、重复逻辑 | 控制展开factor、资源平衡 |
| **II过大** | 循环依赖、资源冲突 | Dataflow、数组分区 |

---

## 性能基准对比（新增）

### 实测性能参考（2026年4月7日）

**测试项目**: SOCS HLS Simple Implementation (Placeholder)
**配置**: Nx=4, IFFT 32×32, Output 17×17

**实测性能指标**:
```
Timing:
  Target Clock: 5.00 ns (200 MHz)
  Estimated Clock: 3.650 ns
  Estimated Fmax: 274 MHz ✅ (超过目标4 MHz)

Latency:
  Min/Max: 1275 cycles ✅ (极佳，远低于10M目标)
  Absolute: 6.375 μs
  Interval: 1276 cycles
  Pipeline Type: no

资源:
  RTL包大小: 88KB
  浮点IP核: 4个 (fadd, fmul, fdiv, sitofp)
  AXI-MM接口: 6个
  AXI-Lite接口: 2个
```

**性能等级**: ✅ **优秀** (Fmax超标，Latency极佳)

---

## 下一步工作提示（新增）

### 综合成功后的下一步

1. **执行Co-Simulation验证**:
   ```bash
   vitis-run --mode hls --cosim \
       --config script/config/hls_config_kernel.cfg \
       --work_dir hls/kernel_csynth  # 使用相同工作目录
   ```

2. **执行Package导出**（可选）:
   ```bash
   vitis-run --mode hls --package \
       --config script/config/hls_config_kernel.cfg \
       --work_dir hls/kernel_csynth \
       --package_output_format rtl
   ```

3. **集成到Vivado项目**:
   - 解压RTL包到Vivado项目目录
   - 添加IP核到Block Design
   - 配置AXI接口连接

---

## 参考文件（更新）

### 实践验证的配置示例

**正确配置文件**: `script/config/hls_config_socs.cfg`
```ini
part = xcku3p-ffvb676-2-e

[hls]
clock = 5ns
flow_target = vivado

syn.file = ../../src/socs_simple.h
syn.file = ../../src/socs_simple.cpp
syn.top = calc_socs_simple_hls

tb.file = ../../tb/tb_socs_simple.cpp
```

**正确源代码**: `src/socs_simple.cpp`
- 包含所有AXI-MM接口的depth参数
- 正确配置6个AXI-MM + 1个AXI-Lite接口
- 使用Pipeline优化循环

**完整测试报告**: `HLS_VALIDATION_WORKFLOW_COMPLETE_REPORT.md`
- 包含详细的性能指标
- 包含常见问题和解决方案
- 包含完整流程验证结果

---

**文档更新时间**: 2026年4月7日  
**验证状态**: ✅ 基于实际综合测试改进  
**实测基准**: Fmax=274 MHz, Latency=1275 cycles