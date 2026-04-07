---
name: hls-full-validation
description: 'WORKFLOW SKILL — Execute complete Vitis HLS validation pipeline: C Simulation → C Synthesis → C/RTL Co-Simulation → Package. USE FOR: end-to-end functional and performance validation; verifying HLS kernels from simulation to deployable IP. Essential for FFT/SOCS kernels requiring Golden data comparison. DO NOT USE FOR: partial validation; board testing; Vivado synthesis.'
---

# Vitis HLS Full Validation Skill

## 适用场景

当用户需要执行以下任务时使用此skill:
- **完整验证流程**: 从C仿真到IP导出的端到端验证
- **功能正确性验证**: 与Golden数据对比确认算法正确
- **性能达标验证**: 确保综合指标和RTL性能满足要求
- **可部署性验证**: 生成可直接集成的IP核

## 核心约束

**项目路径**: `/root/project/FPGA-Litho/source/SOCS_HLS`

**关键约束**:
1. **Nx/Ny动态计算**: 不是配置文件固定值,需根据光学参数计算
2. **IFFT尺寸改写**: 必须使用2的幂次 (32×32 或 128×128 zero-padded)
3. **Golden数据对比**: 必须与Python verification生成的数据一致
4. **验证顺序**: CSim → CSynth → CoSim → Package (严格顺序)

## 执行流程 (必须完整执行)

### 步骤 0: Golden数据准备（前置步骤）

**关键**: 必须先运行calcSOCS_reference生成tmpImgp_pad32.bin

**命令**:
```bash
cd /root/project/FPGA-Litho/verification/src
# 如果calcSOCS_reference已编写完成
make calcSOCS_reference
./calcSOCS_reference
```

**输出**: `output/verification/tmpImgp_pad32.bin` (17×17 float)
- 这是HLS IP的唯一golden输出
- 用于验证HLS算法的中间步骤

**如果calcSOCS_reference尚未完成**: 
- 参考 `source/SOCS_HLS/data/GOLDEN_EXTRACTION_PLAN.md`
- 需要编写该程序生成golden数据

### 步骤 1: C Simulation (功能正确性)

**命令**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --csim \
    --config script/config/hls_config_fft.cfg \
    --work_dir hls/fft_2d_forward_32
```

**验证内容**:
- **关键**: 与tmpImgp_pad32.bin对比（17×17，HLS的直接输出）
- 与完整aerial image对比（512×512，用于端到端验证）
- 确认testbench输出显示 "PASS"
- 检查误差在 `1e-5~1e-4` 量级
- 验证Nx=4, Ny=4参数正确传递

**失败处理**:
- 分析testbench输出和错误日志
- 检查数据格式、存储顺序、计算逻辑
- 确认与litho.cpp CPU reference一致性
- 自动修复或提供具体修改建议

### 步骤 2: C Synthesis (使用hls-csynth-verify skill)

**执行方式**:
- 优先使用 `v++ -c --mode hls` (方案1)
- 或使用 `vitis-run --mode hls --tcl` (方案2)

**验证标准**:
- Fmax ≥ 270 MHz (参考 273.97 MHz)
- Latency达标 (初版 < 10M cycles)
- 资源利用率合理
- 无critical warnings

**优化循环** (如果未达标):
- 自动调用hls-csynth-verify的优化策略
- pipeline、array_partition、unroll等优化
- 持续迭代直到指标达标

### 步骤 3: C/RTL Co-Simulation (必须在综合成功后)

**命令**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --cosim \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

**验证内容**:
- RTL仿真结果与C仿真一致
- 时序行为正确
- 接口交互符合AXI协议
- 误差在允许范围内

**失败处理**:
- 检查RTL与C代码的时序差异
- 分析AXI接口行为
- 确认testbench适配RTL特性
- 提供修复建议和代码修改

### 步骤 4: Package / Export (生成可部署IP)

**配置准备**:
在 `hls_config_fft.cfg` 中确认包含:
```ini
[package]
package.output.format=rtl        # 推荐用于后续集成
# package.output.format=kernel   # 用于Vitis加速流程
# package.output.format=ip       # 传统Vivado IP格式
```

**执行命令**:
```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --package \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

**验证内容**:
- 成功生成RTL包文件
- 包含必要的接口文档
- 验证输出文件完整性

## 验收标准

**整体验收**:
| 验收项 | 标准 |
|--------|------|
| C Simulation | PASS,误差 ≤ 1e-4 |
| C Synthesis | Fmax ≥ 270 MHz,Latency达标 |
| Co-Simulation | RTL与C结果一致,PASS |
| Package | 成功生成RTL/kernel包 |

**项目特定验收**:
- Nx/Ny参数正确计算和传递
- IFFT尺寸满足2的幂次要求
- 与litho.cpp CPU reference一致
- 与Python Golden数据对比通过

## 行为规则

**自动化处理**:
1. 任何步骤失败时立即分析错误日志
2. 尝试自动修复常见问题
3. 记录修复步骤和原因
4. 重新执行失败的步骤
5. 持续迭代直到全部通过

**关键检查点**:
- 每步完成后立即验证输出
- 确认与项目约束一致
- 对比Golden数据确认正确性
- 记录所有关键指标

**失败恢复策略**:
- **CSim失败**: 检查testbench、数据格式、计算逻辑
- **CSynth失败**: 调用hls-csynth-verify优化
- **CoSim失败**: 检查RTL适配、时序、接口
- **Package失败**: 检查配置文件、输出格式

## 最终报告

全部通过后生成清晰总结:
- 各步骤执行结果
- 关键性能指标
- 输出文件路径
- 与Golden数据对比结果
- 下一步建议 (板级验证、系统集成)

## 参考文件

- **CPU Reference**: `verification/src/litho.cpp`
- **Golden数据生成**: `python verification/run_verification.py`
- **Golden文件**: `output/verification/aerial_image_tcc_direct.bin`
- **配置示例**: `reference/tcl脚本设计参考/hls_config_fft.cfg`
- **任务清单**: `source/SOCS_HLS/SOCS_TODO.md`