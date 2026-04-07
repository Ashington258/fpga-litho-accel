---
name: hls-csynth-verify
description: 'WORKFLOW SKILL — Execute Vitis HLS C Synthesis and verify results. USE FOR: running v++ or vitis-run synthesis; checking Fmax/Latency metrics; proposing optimizations when targets not met; iterative refinement until metrics achieved. Ideal for FFT/SOCS high-performance kernels with Vitis 2025.2. DO NOT USE FOR: Vivado RTL synthesis; software simulation; board testing.'
---

# Vitis HLS C Synthesis Verification Skill

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

## 执行流程

### 步骤 1: 选择执行方式

询问用户或自动判断:
- **方案1 (推荐)**: `v++` 一站式命令 - 快速、直观,适合独立FFT组件
- **方案2**: `vitis-run + TCL` - 适合复杂项目、多步流程控制

### 步骤 2: 执行C综合

**方案A（当前配置 Nx=4）** - 32×32 IFFT:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

**预期结果**: Fmax ≈ 273.97 MHz, Latency ≈ 19,407 cycles

**方案B（目标配置 Nx=16）** - 128×128 IFFT（需修改config.json）:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_ifft_128
```

**方案2 - TCL驱动**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls \
    --tcl script/run_csynth_fft.tcl
```

### 步骤 3: 解析综合报告

**必须报告的关键指标**:
- **Estimated Fmax**: 目标 ≥ 270 MHz (参考 273.97 MHz)
- **Latency**: cycles (参考 ≈ 19,407 cycles)
- **资源利用率**: LUT/FF/BRAM/DSP
- **警告/错误**: 任何综合警告或错误信息
- **Nx/Ny一致性**: 是否符合项目配置要求
- **IFFT尺寸**: 是否满足2的幂次要求

### 步骤 4: 性能验证与优化

**验收标准**:
- Fmax ≥ 270 MHz
- Latency在目标范围内 (初版 < 10M cycles, 优化版 < 5M cycles)
- 无critical warnings
- 满足Nx/Ny和IFFT改写约束

**优化策略 (如果未达标)**:

1. **流水线优化**:
   ```cpp
   #pragma HLS PIPELINE II=1
   ```
   - 对nk循环、FFT行/列处理应用pipeline

2. **数组分区**:
   ```cpp
   #pragma HLS ARRAY_PARTITION cyclic factor=4 variable=buffer
   ```
   - 对IFFT buffer、transpose matrix分区

3. **循环展开**:
   ```cpp
   #pragma HLS UNROLL factor=2
   ```
   - 对点乘、累加等计算密集循环展开

4. **Dataflow优化**:
   ```cpp
   #pragma HLS DATAFLOW
   ```
   - 对多个独立步骤实现任务级并行

5. **存储优化**:
   - `krns`和`scales`本地缓存
   - 临时buffer使用BRAM分区

### 步骤 5: 迭代优化循环

**自动化优化流程**:
1. 分析未达标指标
2. 提出具体优化建议和代码修改位置
3. 执行修改
4. 重新运行C综合
5. 验证新指标
6. 循环直到达标或无法继续优化

**终止条件**:
- 所有指标达标
- 达到优化极限 (无法进一步改进)
- 用户手动终止

## 错误处理

**常见问题**:
1. **Fmax不达标**: 检查pipeline、时钟约束、关键路径
2. **资源超额**: 调整数组分区factor、使用资源共享
3. **路径错误**: 确保使用相对路径、在正确目录执行
4. **配置不匹配**: 验证器件、时钟、flow_target设置
5. **IFFT尺寸错误**: 确认使用nextPowerOfTwo()函数

**修复策略**:
- 提供具体代码修改建议
- 给出pragma示例
- 指出需要修改的文件和行号
- 提供预期优化效果

## 下一步建议

完成C综合后,自动建议:
- **进行Co-Simulation**: 验证RTL与C代码一致性
- **导出IP**: Package生成RTL/kernel包
- **性能分析**: 深入分析瓶颈和优化空间

## 参考文件

- **当前CPU实现**: `verification/src/litho.cpp`
- **配置示例**: `reference/tcl脚本设计参考/hls_config_fft.cfg`
- **任务清单**: `source/SOCS_HLS/SOCS_TODO.md`
- **Golden数据**: `output/verification/aerial_image_tcc_direct.bin`