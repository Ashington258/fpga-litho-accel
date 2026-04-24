# FPGA-Litho SOCS_HLS 全局约束与工作流程指南

**项目路径**：`/home/ashington/fpga-litho-accel/source/SOCS_HLS`

本文件定义了本项目中 **HLS 开发、验证、导出** 的标准化流程（已基于 **Vitis 2025.2** 验证通过）。

---

## ⚠️ 核心约束：Golden 数据验证配置一致性

**重要原则**：Golden 数据与 HLS 验证必须使用**完全相同的配置文件**，否则数据无参考价值。

### 验证流程标准化命令

```bash
# 步骤1：生成 Golden 数据（使用指定配置）
cd /home/ashington/fpga-litho-accel && python3 validation/golden/run_verification.py --config input/config/golden_1024.json

# 步骤2：运行 Host 预处理（使用相同配置）
cd /home/ashington/fpga-litho-accel/source/host && python3 run.py --config ../../input/config/golden_1024.json --mode full

# 步骤3：对比验证
python3 validation/compare_hls_golden.py --config input/config/golden_1024.json
```

### 配置一致性检查清单

| 检查项        | Golden (run_verification.py) | Host (run.py)   | HLS TB        |
| ------------- | ---------------------------- | --------------- | ------------- |
| 配置文件      | `--config` 参数              | `--config` 参数 | `config_path` |
| NA            | 必须一致                     | 必须一致        | 必须一致      |
| wavelength    | 必须一致                     | 必须一致        | 必须一致      |
| Lx/Ly         | 必须一致                     | 必须一致        | 必须一致      |
| source type   | 必须一致                     | 必须一致        | 必须一致      |
| σ_inner/outer | 必须一致                     | 必须一致        | 必须一致      |

**推荐配置（按优先级）**:
1. **新架构目标**: `input/config/golden_1024.json` (Lx=1024, Nx≈8, FFT=128×128)
2. **原标准配置**: `input/config/golden_original.json` (Lx=512, Nx≈4, FFT=32×32)

> **每次验证前，必须确认配置一致性，否则验证结果无效！**

---

### 1. 参考资源

| 参考项              | 路径                                                                             | 用途                               |
| ------------------- | -------------------------------------------------------------------------------- | ---------------------------------- |
| **当前 CPU 实现**   | `validation/golden/src/litho.cpp`                                                | **实际运行版本**，使用 65×65 IFFT  |
| HLS FFT 参考实现    | `reference/vitis_hls_ftt的实现/interface_stream/`                                | `hls::fft` 集成模板（路径已修正）  |
| BIN 格式规范        | `input/BIN_Format_Specification.md`                                              | 输入数据格式定义                   |
| **标准配置文件**    | `input/config/golden_original.json`                                              | **验证用统一配置**                 |
| **Golden 数据生成** | `python validation/golden/run_verification.py --config <json>`                   | **生成 mskf_r.bin, scales.bin 等** |
| **Host 预处理**     | `source/host/run.py --config <json>`                                             | **FPGA输入数据生成**               |
| **Golden 参考**     | `output/verification/aerial_image_tcc_direct.bin`                                | **TCC 直接成像（理论标准）**       |
| 任务清单            | `source/SOCS_HLS/SOCS_TODO.md`                                                   | 详细任务分解与进度追踪             |
| HLS Config 示例     | `reference/tcl脚本设计参考/hls_config_fft.cfg`                                   | `v++` / `vitis-run` 配置文件模板   |
| TCL 综合脚本示例    | `reference/tcl脚本设计参考/run_csynth_fft.tcl`                                   | TCL 驱动的综合流程模板             |
| 命令与流程参考      | `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md` | `vitis-run` 命令、TCL 板级验证流程 |
| TCL 脚本模板        | `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl`                       | JTAG-to-AXI 操作模板               |

### 2. 严格控制的 HLS 命令规范（已验证）

**推荐工作目录**（所有命令均在此目录下执行）：

```bash
cd /home/ashington/fpga-litho-accel/source/SOCS_HLS
```

### ⚠️ 关键约束说明

**Nx, Ny 参数特性**：

- **Nx, Ny 不是配置参数**，而是根据光学参数动态计算
- **计算公式**：$N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$

**配置对比（2026-04-24更新）**：

| 配置文件        | Lx   | NA  | λ   | σ_outer | Nx计算 | 实际Nx | FFT尺寸 |
|---------------|------|-----|-----|---------|--------|--------|---------|
| golden_original | 512  | 0.8 | 193 | 0.9     | $\lfloor\frac{0.8×512×1.9}{193}\rfloor$ | **4**  | 32×32   |
| golden_1024   | 1024 | 0.8 | 193 | 0.9     | $\lfloor\frac{0.8×1024×1.9}{193}\rfloor$ | **8**  | 128×128 |
| config_Nx16   | 1536 | 0.8 | 193 | 0.9     | $\lfloor\frac{0.8×1536×1.9}{193}\rfloor$ | **12** | 128×128 |

**新架构目标**：
- **golden_1024配置**：Lx=1024 → Nx≈8，FFT尺寸128×128（支持Nx最大到24）
- **优势**：分辨率提升4倍，支持更大Nx范围

---

### 📊 存储架构说明（2025-02更新）

**AXI-MM接口DDR架构**：

当前HLS IP已配置**6个独立AXI-MM Master接口**访问外部DDR内存：

```cpp
// socs_simple.cpp - AXI-MM接口配置（DDR访问）
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0 depth=262144 latency=32
#pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1 depth=262144 latency=32
#pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem2 depth=10
#pragma HLS INTERFACE m_axi port=krn_r  offset=slave bundle=gmem3 depth=810
#pragma HLS INTERFACE m_axi port=krn_i  offset=slave bundle=gmem4 depth=810
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem5 depth=289
```

| 接口  | 数据类型            | 尺寸       | DDR地址空间用途   |
| ----- | ------------------- | ---------- | ----------------- |
| gmem0 | mskf_r (mask频域实) | 512×512    | 输入mask spectrum |
| gmem1 | mskf_i (mask频域虚) | 512×512    | 输入mask spectrum |
| gmem2 | scales (特征值)     | nk=10      | SOCS特征值权重    |
| gmem3 | krn_r (kernel实)    | nk×9×9=810 | SOCS kernel数据   |
| gmem4 | krn_i (kernel虚)    | nk×9×9=810 | SOCS kernel数据   |
| gmem5 | output (输出图像)   | 17×17=289  | 最终空中像输出    |

**内部数组存储策略**：

当前实现中，**内部计算数组仍使用BRAM**（通过RESOURCE pragma指定）：

```cpp
#pragma HLS RESOURCE variable=fft_input  core=RAM_2P_BRAM  // 32×32 complex
#pragma HLS RESOURCE variable=fft_output core=RAM_2P_BRAM  // 32×32 complex
#pragma HLS RESOURCE variable=tmpImg     core=RAM_2P_BRAM  // 32×32 float
#pragma HLS RESOURCE variable=tmpImgp    core=RAM_2P_BRAM  // 32×32 float
```

**资源利用率问题（xcku5p-ffvb676-2-e）**：

| 资源类型 | 使用量  | 可用量  | 占用率 | 状态    |
| -------- | ------- | ------- | ------ | ------- |
| BRAM     | 1,366   | 960     | 142%   | ❌ 超限 |
| DSP      | 8,080   | 1,824   | 442%   | ❌ 超限 |
| FF       | 556,361 | 433,920 | 128%   | ❌ 超限 |
| LUT      | 647,932 | 216,960 | 298%   | ❌ 超限 |

> **主要DSP消耗源**：`fft_2d_rows`函数使用直接DFT计算（非HLS FFT IP），消耗8,064 DSP

---

### 🖥️ 环境配置说明（2026-04-21更新）

**实际开发环境**: **Windows 10/11 + Vivado/Vitis 2025.2**

**重要说明**: 本项目在**Windows环境**下开发和验证，无需切换到Linux环境。

**硬件验证状态**:

- **2026-04-19**: 硬件验证通过（DDR + HLS IP 正常）
- JTAG连接: localhost:3121
- 目标器件: `xcku3p-ffvb676-2-e` (全局约束中xcku5p为旧版本，请使用xcku3p)

**Vivado TCL Console注意事项**:

1. 命令格式: `run_hw_axi [get_hw_axi_txns <txn>]` (新版格式)
2. 地址必须为十六进制: `format "0x%08X"`
3. MAX_BURST_LEN: 128 (JTAG-to-AXI限制)
4. JTAG-to-AXI burst特性: 数据会反序写入内存，需反序写入抵消

---

### 🔧 FFT优化方案（2026-04-22更新）

**问题根源**：当前FFT实现采用直接DFT计算，而非Vitis HLS FFT IP，导致DSP消耗极高（8,064 DSP，442%占用率）。

**⚠️ 最新验证结果**：C仿真输出全为0，需先解决数据流问题。

**推荐优化方案（按优先级排序）**：

#### ⭐ 方案1：集成HLS FFT IP（最高优先级，用户要求）

**参考实现**：`reference/vitis_hls_ftt的实现/interface_stream/fft_top.cpp`

**预期效果**：
- **DSP消耗**：从8,064降至~1,600（降低80%+，88%占用率）
- **Latency**：从O(N²)降至O(N log N)，C仿真从454s降至~45s（降低10倍+）
- **Fmax**：从273MHz提升至300MHz+

**实现步骤**：
1. 定义FFT配置结构体：
   ```cpp
   struct fft_config_2048_t {
       static const unsigned FFT_NFFT_MAX = 7;  // log2(128)
       static const unsigned FFT_LENGTH = 128;  // Fixed 128-point
       static const bool FFT_HAS_NFFT = false;
       static const bool FFT_HAS_OUT_RDY = true;
   };
   ```

2. 替换`fft_1d_direct_2048()`为stream-based `hls::fft`：
   ```cpp
   void fft_1d_hls_2048(
       hls::stream<cmpx_2048_t> &in_stream,
       hls::stream<cmpx_2048_t> &out_stream,
       bool is_inverse
   ) {
       #pragma HLS DATAFLOW
       #pragma HLS STREAM variable=in_stream depth=128
       #pragma HLS STREAM variable=out_stream depth=128
       
       hls::fft<fft_config_2048_t>(in_stream, out_stream, is_inverse ? 1 : 0);
   }
   ```

3. 重构`fft_2d_full_2048()`使用stream pipeline：
   ```cpp
   void fft_2d_stream_2048(
       cmpx_2048_t input[128][128],
       cmpx_2048_t output[128][128],
       bool is_inverse
   ) {
       #pragma HLS DATAFLOW
       
       // Row FFT with stream
       hls::stream<cmpx_2048_t> row_stream("row_stream");
       hls::stream<cmpx_2048_t> col_stream("col_stream");
       
       // Pipeline stages
       fft_rows_stream(input, row_stream, is_inverse);
       transpose_stream(row_stream, col_stream);
       fft_cols_stream(col_stream, output, is_inverse);
   }
   ```

**验证要求**：
- RMSE < 1e-5（对比FFTW）
- DSP占用率 < 90%
- C仿真时间 < 60s

#### ⭐ 方案2：Kernel并行化（用户要求）

**当前状态**：nk=10顺序循环处理，每个kernel需~45s（C仿真）

**优化目标**：
- 使用DATAFLOW pragma实现kernel并行处理
- 创建多个FFT实例并行处理不同kernel

**预期效果**：
- **吞吐量提升**：接近10倍
- **Latency降低**：从10×45s降至~45s（理论值）

**实现示例**：
```cpp
void calc_socs_parallel_2048(...) {
    #pragma HLS DATAFLOW
    
    // 并行处理10个kernel
    for (int k = 0; k < nk; k++) {
        #pragma HLS UNROLL factor=2  // 限制并行度避免资源超限
        
        // 每个kernel独立流水线
        embed_kernel_stream(k, fft_in_stream[k]);
        fft_2d_stream(fft_in_stream[k], fft_out_stream[k], true);
        accumulate_stream(fft_out_stream[k], tmpImg_stream[k], scales[k]);
    }
    
    // 最终合并
    merge_intensity(tmpImg_stream, output);
}
```

**实现挑战**：
- **BRAM资源**：需要10份tmpImg缓冲（10×128×128×4B = 640KB，超限）
- **DDR带宽**：并行读取10个kernel数据（需AXI burst优化）
- **折衷方案**：UNROLL factor=2（2个kernel并行），降低资源压力

#### ⭐ 方案3：适配golden_1024.json配置（用户要求）

**新配置参数**（`input/config/golden_1024.json`）：
- **Lx/Ly**: 1024×1024（相比512提升4倍分辨率）
- **NA**: 0.8
- **wavelength**: 193nm
- **σ_outer**: 0.9
- **nk**: 10

**Nx动态计算**：
$$N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor = \lfloor \frac{0.8 \times 1024 \times 1.9}{193} \rfloor \approx 8$$

**对应尺寸**：
- **kerX**: 2×Nx+1 = 17
- **convX**: 4×Nx+1 = 33
- **FFT尺寸**: 128×128（固定尺寸，支持Nx最大到24）

**优势**：
- 分辨率提升（1024×1024 mask），成像质量更好
- FFT尺寸固定（128×128），支持更大Nx范围（Nx=2~24）

**验证流程**：
```bash
# 生成golden数据
python3 validation/golden/run_verification.py --config input/config/golden_1024.json

# 预期输出文件
# - mskf_r/i.bin: 1024×1024×4B = 4MB（相比512×512提升4倍）
# - scales.bin: nk×4B = 40B
# - kernels_r/i: nk×17×17×4B = 11.56KB
# - tmpImgp_full_128.bin: 128×128×4B = 65.5KB
```

#### 方案4：使用LUT替代DSP（备选）

适用于DSP严重受限但LUT资源充足的场景：

```cpp
#pragma HLS bind_storage variable=fft_input type=RAM_2P impl=LUTRAM
```

- **DSP消耗**：接近0
- **代价**：LUT消耗增加约200%

#### 方案5：降低FFT精度（固定点）（备选）

将float改为ap_fixed：

```cpp
typedef ap_fixed<18, 8> fixed_t; // 18位总精度，8位整数部分
```

- **DSP消耗**：降低约50%
- **需验证**：精度是否满足光刻计算要求（当前RMSE=1.033e-07）

#### 两种推荐的 C 综合（C Synthesis）方式

**方案 1：v++ 一站式命令（推荐用于独立 FFT 组件）**

```bash
# C仿真 + C综合（推荐）
v++ -c --mode hls \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

**预期结果**：Estimated Fmax ≈ 273.97 MHz，Latency ≈ 19,407 cycles

**方案 2：vitis-run + TCL（适合复杂项目、多步流程控制）**

```bash
vitis-run --mode hls \
    --tcl script/run_csynth_fft.tcl
```

**预期结果**：与方案 1 一致，耗时约 39 秒（视机器性能而定）

#### C 仿真（C Simulation）单独运行

```bash
vitis-run --mode hls --csim \
    --config script/config/hls_config_fft.cfg \
    --work_dir hls/fft_2d_forward_32
```

#### C/RTL 联合仿真（Co-Simulation）—— 需先完成综合

```bash
vitis-run --mode hls --cosim \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

#### IP / RTL 导出（Package）

在配置文件 `hls_config_fft.cfg` 中加入以下配置：

```ini
[package]
package.output.format=rtl        # 或 kernel（用于 Vitis 加速）
# package.output.format=ip       # 传统 Vivado IP 格式
```

执行命令：

```bash
vitis-run --mode hls --package \
    --config script/config/hls_config_fft.cfg \
    --work_dir fft_2d_forward_32
```

### 3. 板级验证流程（JTAG-to-AXI）

使用 Vivado Hardware Manager 执行以下 TCL 命令（可在 Tcl Console 中逐条运行或保存为 `.tcl` 脚本）：

```tcl
# 1. 连接硬件并编程
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. Reset JTAG-to-AXI Master
reset_hw_axi [get_hw_axis hw_axi_1]

# 3. 启动 HLS IP（写 ap_start）
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn

# 4. 等待完成（建议添加轮询 ap_done，offset 通常为 0x04，bit[1]）
# 示例轮询（可封装成 proc）：
# while {[expr {[read_hw_axi_txn ...] & 0x2} == 0]} { ... }

# 5. 读取结果（空中像 / 输出数据）
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 128 -type read
run_hw_axi_txn rd_txn
```

**注意事项**：

- 地址 `0x40000000` 为 HLS IP 的 AXI-Lite Control 接口基地址（根据实际生成报告调整）。
- 建议为 ap_done 轮询增加超时保护，避免无限等待。
- 输出数据长度（`-len 128`）请根据具体 IP 的输出 buffer 大小修改。

### 4. Golden 数据生成流程

**生成测试数据**（从实际配置参数）：

```powershell
cd e:\fpga-litho-accel
python validation/golden/run_verification.py
```

**生成的关键文件**（位于 `output/verification/`）：

- `mskf_r.bin`, `mskf_i.bin`：mask 频域数据（512×512 complex<float>）
- `scales.bin`：特征值（nk 个 float，nk=10）
- `aerial_image_tcc_direct.bin`：TCC 直接成像（512×512 float）
- `aerial_image_socs_kernel.bin`：SOCS kernel 重建成像（512×512 float）

**注意**：当前 litho.cpp 输出是完整的 512×512 aerial image，不是 65×65 tmpImgp。
**HLS IP 需要的中间输出 tmpImgp[65×65] 需从改写版 CPU reference 提取。**

### 5. Toolcall 命令规范（⚠️ 必须严格遵守）

**背景**：本次项目开发中发现 toolcall 报错，原因在于 Shell 命令格式错误。

**环境**: Windows PowerShell（非Linux Bash）

#### ⚠️ 禁止的错误格式

```bash
# ❌ 错误示例1：中文引号与英文引号冲突
echo "启动C综合..." exit_code: $?   # 中文引号""，命令结构不完整

# ❌ 错误示例2：使用 Shell 特有变量
echo "执行完成，退出码: $?"         # $? 是 shell 变量，toolcall 中无法解析

# ❌ 错误示例3：多行命令未正确拼接
echo "启动任务"
sleep 10
echo "任务完成"                    # 应使用 && 或 ; 拼接
```

#### ✅ 正确格式规范

**单行命令**：

```powershell
# ✅ 正确：简单命令（无引号冲突）
cd e:\fpga-litho-accel\source\SOCS_HLS; ls

# ✅ 正确：使用英文引号
echo "Starting C Synthesis..."

# ✅ 正确：避免中文引号，使用转义或变量
message="C Synthesis started" && echo "$message"
```

**多行命令**：

```powershell
# ✅ 正确：使用 ; 拼接
cd e:\fpga-litho-accel\source\SOCS_HLS; v++ -c --mode hls --config script/config/hls_config.cfg; echo "C Synthesis completed"

# ✅ 正确：复杂命令建议使用 run_in_terminal 工具
# run_in_terminal工具会自动处理命令执行
```

#### 🔑 关键原则

1. **禁止中文引号**：所有 toolcall 命令字符串必须使用 **英文引号** `""`
2. **禁止 Shell 变量**：不要在 toolcall 中使用 `$?`, `$PWD`, `$HOME`, `$$` 等 Shell 特有变量
3. **单行命令优先**：复杂逻辑拆分为多个独立的 toolcall，或使用 `run_in_terminal` 工具
4. **命令完整性**：每个 toolcall 命令必须是完整、可执行的单行 shell 命令
5. **路径使用绝对路径**：toolcall 中避免使用相对路径，推荐使用绝对路径

#### 📝 HLS 命令专用规范

**正确的 HLS 综合命令**（Windows PowerShell）：

```powershell
# ✅ 方案1：v++ 一站式命令
cd e:\fpga-litho-accel\source\SOCS_HLS; v++ -c --mode hls --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp

# ✅ 方案2：vitis-run + TCL
cd e:\fpga-litho-accel\source\SOCS_HLS; vitis-run --mode hls --tcl script/run_csynth_socs_full.tcl

# ✅ 方案3：C 仿真（使用 --csim 参数）
cd e:\fpga-litho-accel\source\SOCS_HLS; vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg --work_dir hls/socs_full_csim

# ✅ 方案4：Co-Simulation（使用 --cosim 参数）
cd e:\fpga-litho-accel\source\SOCS_HLS; vitis-run --mode hls --cosim --config script/config/hls_config_socs_full.cfg --work_dir socs_full_comp
```

**⚠️ 注意事项**：

- **不要使用** `vitis-run --csynth`（此命令不存在）
- **不要使用** `vitis-run --synth`（此命令不存在）
- C综合的正确方式是：`v++ -c` 或 `vitis-run --tcl` + `csynth_design` TCL 命令
- 所有命令路径建议使用绝对路径或确保在正确目录下执行

### 使用建议

- **日常开发**：优先使用 **方案 1（v++）**，速度快、输出直观。
- **CI/CD 或自动化流程**：使用 **方案 2（TCL）**，便于加入自定义步骤。
- 所有配置文件路径建议使用相对路径，避免硬编码绝对路径。
- 每次修改代码后，建议先跑 **C Simulation** → **C Synthesis** → **Co-Simulation**，确认无误后再导出 IP。
- 如遇到 Fmax 不达标或 CoSim 失败，请检查 `hls_config_fft.cfg` 中的 `clock`、`part`、`flow_target` 等关键设置。
- **严格遵守 Toolcall 命令规范**，避免因格式错误导致工具调用失败。
