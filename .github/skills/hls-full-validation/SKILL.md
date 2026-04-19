---
name: hls-full-validation
description: "WORKFLOW SKILL — Execute complete Vitis HLS validation pipeline: C Simulation → C Synthesis → C/RTL Co-Simulation → Package. USE FOR: end-to-end functional and performance validation; verifying HLS kernels from simulation to deployable IP. Essential for FFT/SOCS kernels requiring Golden data comparison. DO NOT USE FOR: partial validation; board testing; Vivado synthesis."
---

# Vitis HLS Full Validation Skill (Improved)

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

## ⚠️ 关键经验教训（新增）

### 实践中发现的常见问题

| 问题类型              | 典型错误                                                                         | 解决方案                                                                                           |
| --------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| **配置文件解析错误**  | `unrecognised option 'package.package.output.format'`                            | 配置文件中不应包含 `[package]` section，仅在package步骤通过命令行指定                              |
| **数据路径错误**      | `Cannot open file: data/mskf_r.bin`                                              | C Simulation实际执行目录为 `hls/<work_dir>/hls/csim/build/`，正确相对路径为 `../../../../../data/` |
| **AXI-MM depth缺失**  | `A depth specification is required for interface port 'mskf_r' for cosimulation` | AXI-MM接口必须添加 `depth=<size>` 参数才能执行Co-Simulation                                        |
| **头文件include失败** | `fatal error: 'socs_simple.h' file not found`                                    | 使用extern声明或在配置文件中添加头文件到syn.file                                                   |

### 关键配置注意事项

1. **AXI-MM接口必须指定depth**:

   ```cpp
   #pragma HLS INTERFACE m_axi port=data_array offset=slave bundle=gmem depth=ARRAY_SIZE
   ```

2. **配置文件正确格式**:

   ```ini
   # 正确格式（无[package] section）
   part = xcku3p-ffvb676-2-e

   [hls]
   clock = 5ns
   flow_target = vivado

   syn.file = ../../src/kernel.cpp
   syn.file = ../../src/kernel.h  # 添加头文件避免include问题
   syn.top = kernel_function

   tb.file = ../../tb/tb_kernel.cpp  # 使用tb.file而非deprecated sim.file
   ```

3. **数据文件路径计算**:

   ```
   HLS工作目录结构:
   hls/<work_dir>/
   ├── hls/csim/build/csim.exe  <- C Simulation实际执行位置
   ├── hls/syn/                  <- C Synthesis结果
   └── reports/

   正确相对路径（从build目录）:
   ../../../../../data/ -> source/SOCS_HLS/data/
   ```

---

## ⚠️ Toolcall 命令规范（必须严格遵守）

**背景**：项目开发中发现 toolcall 报错，原因在于 Shell 命令格式错误。

### 禁止的错误格式

```bash
# ❌ 错误示例1：中文引号与英文引号冲突
echo "启动C仿真..." exit_code: $?   # 中文引号""，命令结构不完整

# ❌ 错误示例2：使用 Shell 特有变量
echo "执行完成，退出码: $?"         # $? 是 shell 变量，toolcall 中无法解析

# ❌ 错误示例3：多行命令未正确拼接
echo "启动任务"
sleep 10
echo "任务完成"                    # 应使用 && 或 ; 拼接
```

### ✅ 正确格式规范

**单行命令**：

```bash
# ✅ 正确：简单命令（无引号冲突）
cd /root/project/FPGA-Litho/source/SOCS_HLS && ls -la

# ✅ 正确：使用英文引号
echo "Starting C Simulation..."

# ✅ 正确：避免中文引号，使用转义或变量
message="C Simulation started" && echo "$message"
```

**多行命令**：

```bash
# ✅ 正确：使用 && 拼接（前一个成功才执行下一个）
cd /root/project/FPGA-Litho/source/SOCS_HLS && \
vitis-run --mode hls --csim --config script/config/hls_config.cfg && \
echo "C Simulation completed"

# ✅ 正确：使用 ; 拼接（无论成功与否都继续）
cd /root/project/FPGA-Litho/source/SOCS_HLS ; \
ls -la ; \
pwd

# ✅ 正确：复杂命令建议使用 run_in_terminal 工具
run_in_terminal(command="cd /root/project/FPGA-Litho/source/SOCS_HLS && vitis-run --mode hls --tcl script/run_csynth.tcl")
```

### 🔑 关键原则

1. **禁止中文引号**：所有 toolcall 命令字符串必须使用 **英文引号** `""`
2. **禁止 Shell 变量**：不要在 toolcall 中使用 `$?`, `$PWD`, `$HOME`, `$$` 等 Shell 特有变量
3. **单行命令优先**：复杂逻辑拆分为多个独立的 toolcall，或使用 `run_in_terminal` 工具
4. **命令完整性**：每个 toolcall 命令必须是完整、可执行的单行 shell 命令
5. **路径使用绝对路径**：toolcall 中避免使用相对路径，推荐使用绝对路径

### 📝 HLS 全流程验证命令规范

**正确的完整验证流程命令**：

```bash
# ✅ 步骤1：C Simulation
cd /root/project/FPGA-Litho/source/SOCS_HLS && vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg --work_dir hls/socs_full_csim

# ✅ 步骤2：C Synthesis（方案1：v++）
cd /root/project/FPGA-Litho/source/SOCS_HLS && v++ -c --mode hls --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp

# ✅ 步骤2：C Synthesis（方案2：vitis-run + TCL）
cd /root/project/FPGA-Litho/source/SOCS_HLS && vitis-run --mode hls --tcl script/run_csynth_socs_full.tcl

# ✅ 步骤3：Co-Simulation
cd /root/project/FPGA-Litho/source/SOCS_HLS && vitis-run --mode hls --cosim --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp

# ✅ 步骤4：Package/Export IP
cd /root/project/FPGA-Litho/source/SOCS_HLS && vitis-run --mode hls --package --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp
```

**⚠️ 注意事项**：

- **不要使用** `vitis-run --csynth`（此命令不存在）
- **不要使用** `vitis-run --synth`（此命令不存在）
- C综合的正确方式是：`v++ -c` 或 `vitis-run --tcl` + `csynth_design` TCL 命令
- 所有命令路径建议使用绝对路径或确保在正确目录下执行

---

## 执行流程 (必须完整执行)

### 步骤 0: Golden数据准备（前置步骤）✅ 已完成

**状态**: ✅ Golden数据已生成并验证通过（2026年4月7日）

**已生成的Golden文件**（位于 `source/SOCS_HLS/data/`）:

- `mskf_r.bin`, `mskf_i.bin`: 512×512 complex<float> (Mask频谱)
- `scales.bin`: 10 eigenvalues (特征值权重)
- `kernels/krn_k_r.bin`, `krn_k_i.bin`: 10×9×9 complex<float> (SOCS核)
- `tmpImgp_pad32.bin`: **17×17 = 289 floats** (HLS Golden输出) ⭐

**验证状态**:

- ✅ SOCS vs TCC相对差异: 4.07% (PASS, < 10%容差)
- ✅ 数据数值范围合理，无异常值
- ✅ 所有数据已复制到HLS项目目录

**如果需要重新生成**:

```bash
cd /root/project/FPGA-Litho
python validation/golden/run_verification.py
# 生成位置: output/verification/
# 需手动复制到 source/SOCS_HLS/data/
```

---

### 步骤 1: C Simulation (功能正确性)

#### 1.1 配置文件准备

**创建配置文件** `script/config/hls_config_kernel.cfg`:

```ini
# HLS配置文件（正确格式）
part = xcku3p-ffvb676-2-e

[hls]
clock = 5ns
flow_target = vivado

# 源文件（包含头文件）
syn.file = ../../src/kernel.h
syn.file = ../../src/kernel.cpp
syn.top = kernel_function

# Testbench（使用tb.file）
tb.file = ../../tb/tb_kernel.cpp
```

**⚠️ 注意**:

- 不要在配置文件中添加 `[package]` section
- 使用 `tb.file` 替代deprecated的 `sim.file`
- 添加头文件到 `syn.file` 避免编译错误

#### 1.2 Testbench数据路径处理

**Testbench中的正确数据路径**:

```cpp
// Testbench位于: hls/<work_dir>/hls/csim/build/
// 正确相对路径:
load_binary_data("../../../../../data/mskf_r.bin", mskf_r, Lx*Ly);
load_binary_data("../../../../../data/tmpImgp_pad32.bin", golden, 289);
```

**如果使用绝对路径**（推荐用于避免混淆）:

```cpp
load_binary_data("/root/project/FPGA-Litho/source/SOCS_HLS/data/mskf_r.bin", mskf_r, Lx*Ly);
```

#### 1.3 执行C Simulation

**命令**:

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --csim \
    --config script/config/hls_config_kernel.cfg \
    --work_dir hls/kernel_csim
```

**预期输出**:

```
INFO: [SIM 211-2] *************** CSIM start ***************
INFO: [SIM 211-1] CSim done with 0 errors.
```

**验证内容**:

- ✅ 成功加载所有Golden数据（mskf, scales, kernels, tmpImgp）
- ✅ 输出尺寸正确 (17×17 = 289 floats)
- ✅ 输出数值范围合理（非全零或异常值）
- ✅ 确认testbench输出显示 "PASS"

**常见失败处理**:

| 错误类型     | 错误信息                                   | 解决方案                                         |
| ------------ | ------------------------------------------ | ------------------------------------------------ |
| 文件找不到   | `Cannot open file: data/mskf_r.bin`        | 检查相对路径是否为 `../../../../../data/`        |
| 头文件找不到 | `fatal error: 'kernel.h' file not found`   | 添加头文件到配置文件的syn.file，或使用extern声明 |
| 配置解析错误 | `unrecognised option 'package.xxx'`        | 删除配置文件中的 `[package]` section             |
| 编译错误     | `undefined reference to 'kernel_function'` | 检查函数名称是否与syn.top一致                    |

---

### 步骤 2: C Synthesis (性能验证)

#### 2.1 AXI-MM接口depth参数配置 ⭐ 关键

**必须在源代码中添加depth参数**:

```cpp
void kernel_function(
    float *input_data,   // AXI-MM端口
    float *output_data   // AXI-MM端口
) {
    // ⚠️ 必须添加depth参数，否则Co-Simulation会失败
    #pragma HLS INTERFACE m_axi port=input_data offset=slave bundle=gmem0 depth=INPUT_SIZE
    #pragma HLS INTERFACE m_axi port=output_data offset=slave bundle=gmem1 depth=OUTPUT_SIZE

    #pragma HLS INTERFACE s_axilite port=return bundle=control
}
```

**depth参数说明**:

- `depth=<数组大小>`: 指定AXI-MM接口访问的最大深度
- 对于固定大小数组，使用编译时常量（如 `depth=512*512`）
- 对于动态大小，使用宏定义或常量表达式

#### 2.2 执行C Synthesis

**推荐方式1: 使用v++命令** (快速):

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
v++ -c --mode hls \
    --config script/config/hls_config_kernel.cfg \
    --work_dir hls/kernel_csynth
```

**推荐方式2: 使用TCL脚本** (详细控制):

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls \
    --tcl script/run_csynth.tcl
```

**⚠️ 注意**:

- `vitis-run --csynth` 命令不存在，应使用 `v++ -c` 或 `vitis-run --tcl`
- 修改源代码后（如添加depth参数）必须重新执行综合

#### 2.3 验证综合结果

**检查关键性能指标**:

```bash
# 从verbose报告中提取关键信息
cat hls/kernel_csynth/hls/.autopilot/db/kernel_function.verbose.rpt | grep -A 20 "Timing"
cat hls/kernel_csynth/hls/.autopilot/db/kernel_function.verbose.rpt | grep -A 20 "Latency"
```

**性能验收标准**:
| 指标 | 目标值 | 如何提取 |
|------|--------|---------|
| **Estimated Fmax** | ≥ 270 MHz | `Estimated Clock = 3.65 ns → Fmax = 274 MHz` |
| **Latency** | < 10M cycles (初版) | `Latency (cycles) min/max` |
| **Interval** | 合理值 | `Interval (cycles) min/max` |
| **Pipeline** | 确认优化 | `Pipeline Type` |

**综合成功标志**:

- ✅ 生成 `kernel_function.zip` (RTL包)
- ✅ 包含 `hdl/verilog/*.v` 文件
- ✅ 包含 `hdl/ip/*.xci` (浮点运算IP核)
- ✅ 包含 `component.xml` (IP-XACT描述)

**如果性能未达标**:

- 调用 `hls-csynth-verify` skill的优化策略
- 应用pipeline、array_partition、unroll优化
- 重新综合并验证性能改进

---

### 步骤 3: C/RTL Co-Simulation ⭐ 关键步骤

#### 3.1 前置条件检查

**必须满足**:

- ✅ C Synthesis已成功完成（存在 `hls/syn/` 目录）
- ✅ AXI-MM接口已添加depth参数
- ✅ 源代码无语法错误

#### 3.2 执行Co-Simulation

**命令**:

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --cosim \
    --config script/config/hls_config_kernel.cfg \
    --work_dir hls/kernel_csynth  # 使用已有的synthesis工作目录
```

**⚠️ 注意**:

- 必须使用包含synthesis结果的工作目录（`work_dir`应指向已执行C Synth的目录）
- 如果重新编译，需先执行C Synthesis再执行Co-Simulation

#### 3.3 Co-Simulation验证内容

**预期结果**:

- ✅ RTL仿真与C仿真结果一致
- ✅ 时序行为正确
- ✅ AXI接口交互符合协议
- ✅ 输出误差在允许范围内 (< 1e-4)

**常见失败处理**:

| 错误类型      | 错误信息                                                                      | 解决方案                                           |
| ------------- | ----------------------------------------------------------------------------- | -------------------------------------------------- |
| depth缺失     | `A depth specification is required for interface port 'xxx' for cosimulation` | 在AXI-MM接口pragma中添加 `depth=<size>`            |
| syn目录缺失   | `Must run -vppflow 'syn' before 'cosim' (missing file hls/syn)`               | 先执行C Synthesis，或使用包含syn结果的工作目录     |
| RTL行为不一致 | `C TB simulation failed, nonzero return value`                                | 检查RTL与C代码的时序差异，确认testbench适配RTL特性 |
| 时序违规      | `Timing violation detected`                                                   | 检查时钟约束，调整代码时序行为                     |

---

### 步骤 4: Package / Export (生成可部署IP)

#### 4.1 执行Package命令

**命令**:

```bash
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --package \
    --config script/config/hls_config_kernel.cfg \
    --work_dir hls/kernel_csynth \
    --package_output_format rtl  # 指定输出格式
```

**输出格式选择**:

- `rtl`: 推荐，用于Vivado集成
- `kernel`: 用于Vitis加速流程
- `ip_catalog`: 传统Vivado IP格式

**⚠️ 注意**:

- 使用命令行参数 `--package_output_format` 指定格式，而非配置文件中的 `[package]` section
- Package依赖Co-Simulation成功完成

#### 4.2 验证Package输出

**检查生成的文件**:

```bash
ls -lh hls/kernel_csynth/*.zip
unzip -l kernel_function.zip | head -20
```

**预期内容**:

- ✅ `component.xml`: IP-XACT组件描述
- ✅ `hdl/verilog/*.v`: RTL代码
- ✅ `hdl/ip/*.xci`: 浮点运算IP核
- ✅ `constraints/*.xdc`: 时序约束

---

## 验收标准（改进版）

### 完整验收检查清单

| 步骤              | 验收项     | 详细标准                 | 如何验证                                     |
| ----------------- | ---------- | ------------------------ | -------------------------------------------- |
| **Golden数据**    | 数据完整性 | 所有文件存在且大小正确   | `ls -lh source/SOCS_HLS/data/`               |
| **C Simulation**  | 编译成功   | 无编译错误，生成csim.exe | `INFO: [SIM 211-1] CSim done with 0 errors.` |
| **C Simulation**  | 数据加载   | 成功加载所有Golden数据   | Testbench输出显示 "✓ Mask spectrum loaded"   |
| **C Simulation**  | 输出正确   | 输出尺寸和数值范围合理   | Output size: 17×17 = 289 floats              |
| **C Synthesis**   | 性能达标   | Fmax ≥ 270 MHz           | `Estimated Clock ≤ 3.70 ns`                  |
| **C Synthesis**   | RTL生成    | 生成完整Verilog代码      | 存在 `hdl/verilog/*.v`                       |
| **C Synthesis**   | AXI-MM配置 | depth参数正确            | 检查源代码pragma                             |
| **Co-Simulation** | RTL一致    | RTL与C结果一致           | `PASS: CoSim finished successfully`          |
| **Co-Simulation** | 时序正确   | 无timing violation       | 检查CoSim日志                                |
| **Package**       | IP包完整   | 包含所有必要文件         | 解压检查zip内容                              |

### 性能基准参考

**基于实际测试结果（2026年4月7日）**:

- ✅ **Estimated Fmax**: 274 MHz (超过目标4 MHz)
- ✅ **Latency**: 1275 cycles (极佳)
- ✅ **Interval**: 1276 cycles
- ✅ **RTL包大小**: 88KB

---

## 完整流程示例（新增）

### 示例项目: SOCS HLS Simple Implementation

**完整命令序列**:

```bash
# Step 0: Golden数据准备（已完成）
cd /root/project/FPGA-Litho
python validation/golden/run_verification.py
mkdir -p source/SOCS_HLS/data/kernels
cp output/verification/*.bin source/SOCS_HLS/data/
cp output/verification/kernels/*.bin source/SOCS_HLS/data/kernels/

# Step 1: C Simulation
cd /root/project/FPGA-Litho/source/SOCS_HLS
vitis-run --mode hls --csim \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csim

# Step 2: C Synthesis（需先添加depth参数到源代码）
v++ -c --mode hls \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csynth

# Step 3: Co-Simulation（使用synthesis工作目录）
vitis-run --mode hls --cosim \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csynth

# Step 4: Package（可选）
vitis-run --mode hls --package \
    --config script/config/hls_config_socs.cfg \
    --work_dir hls/socs_csynth \
    --package_output_format rtl
```

**预期总耗时**:

- C Simulation: ~7秒
- C Synthesis: ~40秒
- Co-Simulation: ~2-5分钟
- Package: ~10秒

---

## 故障诊断快速参考（新增）

### 常见错误速查表

| 错误关键词                     | 根本原因          | 快速修复                               |
| ------------------------------ | ----------------- | -------------------------------------- |
| `unrecognised option`          | 配置文件格式错误  | 删除 `[package]` section               |
| `Cannot open file`             | 数据路径错误      | 使用 `../../../../../data/` 或绝对路径 |
| `fatal error: file not found`  | 头文件include失败 | 添加头文件到syn.file或使用extern       |
| `depth specification required` | AXI-MM depth缺失  | 添加 `depth=<size>` 参数               |
| `missing file hls/syn`         | CoSim前置条件缺失 | 先执行C Synthesis                      |
| `nonzero return value`         | Testbench失败     | 检查数据加载和计算逻辑                 |
| `Timing violation`             | 时序不满足        | 降低clock频率或优化代码                |

### 逐步排查策略

1. **配置文件问题** → 检查ini格式，删除[package]
2. **编译问题** → 检查syn.file、tb.file、头文件
3. **数据路径问题** → 确认csim.exe执行目录，修正相对路径
4. **AXI-MM问题** → 添加depth参数
5. **性能问题** → 应用pipeline/array_partition优化
6. **CoSim问题** → 确认syn目录存在，检查RTL行为

---

## 参考文件（更新）

### 实践生成的参考文件

**完整验证报告**:

- `source/SOCS_HLS/HLS_VALIDATION_WORKFLOW_COMPLETE_REPORT.md`: 完整测试报告
- `source/SOCS_HLS/hls/VALIDATION_TEST_SUMMARY.md`: 验证流程总结

**配置文件示例**（已验证通过）:

- `script/config/hls_config_socs.cfg`: 正确格式示例

**源代码示例**（含depth参数）:

- `src/socs_simple.cpp`: AXI-MM接口正确配置示例
- `tb/tb_socs_simple.cpp`: Testbench正确数据路径示例

### Golden数据（已生成）

- `data/GOLDEN_DATA_LIST.md`: 完整数据清单
- `data/README.md`: 数据使用指南
- `data/tmpImgp_pad32.bin`: HLS Golden输出 (289 floats)

---

## 最佳实践总结（新增）

### ✅ 推荐做法

1. **配置文件**: 使用正确格式，不包含 `[package]` section
2. **AXI-MM接口**: 必须添加 `depth=<size>` 参数
3. **数据路径**: 使用 `../../../../../data/` 或绝对路径
4. **头文件处理**: 添加到syn.file或使用extern声明
5. **工作目录**: 使用一致的work_dir避免重复编译
6. **流程顺序**: 严格遵循 CSim → CSynth → CoSim → Package

### ❌ 避免的错误

1. 配置文件中添加 `[package]` section
2. AXI-MM接口缺少depth参数
3. 使用deprecated的 `sim.file` 配置项
4. 数据路径使用 `data/` 而非 `../../../../../data/`
5. Co-Simulation使用空工作目录
6. 修改源代码后不重新执行C Synthesis

---

**文档更新时间**: 2026年4月7日  
**验证状态**: ✅ 基于实际测试改进  
**改进来源**: 完整HLS验证流程实践
