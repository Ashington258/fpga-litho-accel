# SOCS HLS 任务清单

**最后更新**: 2026-04-20
**当前状态**: Phase A Host预处理验证完成 (Windows编译+数值验证PASS)
**下一步**: Phase 1 - HLS C Synthesis DSP优化验证

---

## Phase A: Host预处理模块开发 ✅ (已完成代码，待Linux编译验证)

### 任务 A.1: JSON Parser扩展 ✅ (已完成)
- **目标**: 扩展JSON解析器支持完整SOCSConfig参数
- **完成内容**:
  - ✅ 添加Annular, Dipole, CrossQuadrupole, Point结构体
  - ✅ 解析source配置参数
  - ✅ 支持mask inputFile路径解析

### 任务 A.2: source_processor模块 ✅ (已完成)
- **目标**: 实现source pattern生成 (Annular/Dipole/CrossQuadrupole/Point)
- **完成文件**: `source_processor.hpp`, `source_processor.cpp`
- **关键函数**:
  - `createSource()` - 主入口，生成光源矩阵
  - `computeOuterSigma()` - 计算最大光源半径
  - `normalizeSource()` - 归一化能量

### 任务 A.3: mask_processor验证 ✅ (已验证)
- **目标**: 验证现有mask FFT处理模块兼容性
- **状态**: 已有mask_processor.hpp/cpp支持FFTW3

### 任务 A.4: TCC计算模块 ✅ (已完成)
- **目标**: 实现TCC矩阵计算
- **完成文件**: `tcc_processor.hpp`, `tcc_processor.cpp`
- **关键函数**:
  - `calcTCC()` - TCC矩阵积分计算
  - `calcImageFromTCC()` - TCC直接成像方法

### 任务 A.5: Kernel提取模块 ✅ (已完成)
- **目标**: 使用Eigen库提取SOCS kernels (特征值分解)
- **完成文件**: `kernel_extractor.hpp`, `kernel_extractor.cpp`
- **关键函数**:
  - `extractKernels()` - Eigen Hermitian分解
  - `reconstructKernels2D()` - 1D→2D kernel转换

### 任务 A.6: socs_host集成 ✅ (已完成接口修复)
- **目标**: 整合预处理流水线到socs_host主程序
- **修复内容**:
  - ✅ 修复`computeOuterSigma(source)` → `computeOuterSigma(source, config.srcSize)`
  - ✅ 修复`generateSource()` → `createSource()`
- **编译状态**: ✅ Windows编译成功 (FFTW3/Eigen库链接正确)

### 任务 A.7: Host vs Golden数值验证 ✅ (已完成)
- **目标**: 验证Host预处理计算正确性
- **验证配置**: `input/config/golden_original.json`
- **验证结果**:
  - ✅ FFT匹配: mskf_r/i.bin 最大差异 1.49e-08
  - ⚠️ TCC差异: ~0.1% (实现模式差异，可接受)
  - ⚠️ 特征值差异: λ[0]=0.057%, λ[1-3]=0.082-0.092% (可接受)
- **根因分析**:
  - Golden: 预计算pupil矩阵 → 存储后使用
  - Host: 实时计算pupil (lambda函数)
  - 两者数学等价，浮点累积模式不同
- **结论**: 差异可接受 (Golden自身允许3.3%截断误差)
- **诊断脚本**: `validation/diagnose_tcc_difference.py`

---

## Phase 0: 源代码整理 ✅ (已完成)

### 任务 0.1: 文件结构整理 ✅
- **目标**: 统一HLS源代码，删除冗余文件
- **完成内容**:
  - ✅ 创建 `socs_config.h` - 动态配置系统
    - Nx/Ny动态计算公式: `Nx = floor(NA * Lx * (1+σ_outer) / λ)`
    - FFT尺寸动态推导 (32/64/128)
    - FFT IP配置结构体
  
  - ✅ 更新 `socs_simple.cpp` - 统一实现
    - 使用 `socs_config.h` 配置
    - 删除硬编码Nx值
    - 保持Vitis FFT IP集成
  
  - ✅ 更新 `socs_simple.h` - 统一头文件
  
  - ✅ 最终文件结构:
    ```
    source/SOCS_HLS/src/
      ├── socs_config.h    (动态配置)
      ├── socs_simple.cpp  (主实现)
      └── socs_simple.h    (头文件)
    ```

### 任务 0.2: 配置系统验证 ✅ (已完成)
- **目标**: 验证动态配置编译正确性
- **完成内容**:
  - ✅ C Simulation v3 验证通过 (socs_simple_csim_v3)
  - ✅ C Synthesis v2 验证通过 (socs_simple_synth_v2)
  - ⏳ DSP优化验证 (目标: 8,080 → ~200-400)

---

## Phase 1: FFT DSP优化 ⏳ (进行中)

### 任务 1.0: Fixed-Point FFT 尝试 ❌ (失败)
- **问题**: Vitis HLS FFT IP 内部自动调整 bit-width
  - `input_width=32` → 内部期望 `ap_fixed<40, 1>`
  - 用户定义 `ap_fixed<32, 1>` → 类型不匹配
  - 错误: `no matching function for call to 'fft'`
- **根本原因**: FFT IP 公式 `((_CONFIG_T::input_width+7)/8)*8` 向上对齐到 8-byte boundary

### 任务 1.1: Float + LUT FFT 优化 ✅ (已完成代码修改)
- **目标**: 使用 Float FFT + LUT 模式实现 DSP-free
- **修改内容**:
  - ✅ `socs_config.h`:
    - `FFT_USE_FLOAT = 1` (启用 float FFT)
    - `complex_mult_type = use_luts` (DSP-free)
    - `butterfly_type = use_luts` (DSP-free)
  
  - ✅ `socs_simple.cpp`:
    - 移除 `.to_float()` 方法调用 (float 类型无需转换)
    - 直接使用 `sample.real()` 和 `sample.imag()`

- **预期改善**:
  - DSP: 8,080 → ~0 (100% reduction)
  - LUT: 增加约 200% (需验证可接受性)
  - 精度: Float FFT 精度高于 fixed-point (RMSE < 1e-5)

### 任务 1.2: C Simulation 验证 Float+LUT ⏳ (待执行)
- **命令**:
  ```bash
  cd E:\fpga-litho-accel\source\SOCS_HLS
  vitis-run --mode hls --csim --config script/config/hls_config_socs_simple.cfg --work_dir hls/socs_simple_csim_float_lut_v2
  ```
- **验收标准**:
  - 编译成功 (无 error)
  - 功能正确 (与 Golden 数据对比)

### 任务 1.3: C Synthesis 验证 DSP 改善 ⏳ (待执行)
- **命令**:
  ```bash
  cd E:\fpga-litho-accel\source\SOCS_HLS
  v++ -c --mode hls --config script/config/hls_config_socs_simple.cfg --work_dir socs_simple_synth_float_lut
  ```
- **验收标准**:
  - DSP ≤ 100 (目标: ~0)
  - Fmax ≥ 250 MHz
  - BRAM ≤ 960

---

## Phase 1: HLS综合与优化 (进行中)

### 任务 1.1: C综合验证
- **目标**: 完成C综合并验证资源利用率
- **命令**: 
  ```bash
  cd source/SOCS_HLS
  v++ -c --mode hls --config script/config/hls_config_socs_simple.cfg --work_dir hls_csynth
  ```
- **验收标准**:
  - Estimated Fmax ≥ 250 MHz
  - Latency ≤ 20,000 cycles
  - DSP ≤ 500 (当前16)
  - BRAM ≤ 960 (xcku5p限制)

### 任务 1.2: C/RTL联合仿真
- **目标**: 验证RTL实现与Golden数据一致性
- **命令**:
  ```bash
  vitis-run --mode hls --cosim --config script/config/hls_config_socs_simple.cfg --work_dir hls_cosim
  ```
- **验收标准**:
  - RMSE ≤ 1e-6 (与CPU golden对比)
  - CoSim PASS

### 任务 1.3: IP导出
- **目标**: 导出可集成到Vivado的IP
- **命令**:
  ```bash
  vitis-run --mode hls --package --config script/config/hls_config_socs_simple.cfg --work_dir hls_package
  ```
- **验收标准**:
  - IP-XACT格式正确
  - AXI-MM接口完整

---

## Phase 2: Vivado集成 (待开始)

### 任务 2.1: Block Design创建
- **目标**: 在Vivado中集成HLS IP
- **内容**:
  - 创建AXI互联
  - 配置DDR接口
  - 添加JTAG-to-AXI Master

### 任务 2.2: 板级验证
- **目标**: 在xcku5p板卡上验证
- **参考**: `.github/skills/hls-board-validation/SKILL.md`
- **验收标准**:
  - ap_start触发成功
  - ap_done轮询正常
  - 输出数据与Golden一致

---

## 技术细节

### 配置参数映射 (socs_config.h)

| 参数     | 默认值 | 计算公式             | 说明           |
| -------- | ------ | -------------------- | -------------- |
| Nx       | 4      | floor(NA*Lx*(1+σ)/λ) | 频域截断宽度   |
| convX    | 17     | 4*Nx + 1             | 卷积输出宽度   |
| fftConvX | 32     | nextPow2(convX)      | FFT网格尺寸    |
| kerX     | 9      | 2*Nx + 1             | Kernel支持宽度 |

### FFT实现对比

| 方案         | DSP消耗 | 验证状态 | 说明                |
| ------------ | ------- | -------- | ------------------- |
| 直接DFT      | 8,064   | ❌ 超限   | 旧socs_fft.cpp      |
| HLS FFT IP   | ~400    | ✅ 待验证 | 当前socs_simple.cpp |
| LUT-only FFT | ~0      | ⏳ 可选   | DSP-free方案        |

### 资源利用率目标 (xcku5p-ffvb676-2-e)

| 资源 | 可用量  | 目标上限 | 当前使用 | 状态 |
| ---- | ------- | -------- | -------- | ---- |
| BRAM | 960     | ≤960     | ?        | ⏳    |
| DSP  | 1,824   | ≤500     | 16       | ✅    |
| FF   | 433,920 | ≤200k    | ?        | ⏳    |
| LUT  | 216,960 | ≤150k    | ?        | ⏳    |

---

## 关键文件路径

| 文件类型          | 路径                                                       |
| ----------------- | ---------------------------------------------------------- |
| HLS源码           | `source/SOCS_HLS/src/socs_simple.cpp`                      |
| 配置头文件        | `source/SOCS_HLS/src/socs_config.h`                        |
| Testbench         | `source/SOCS_HLS/tb/tb_socs_simple.cpp`                    |
| HLS Config        | `source/SOCS_HLS/script/config/hls_config_socs_simple.cfg` |
| Golden数据        | `output/verification/`                                     |
| 输出mask spectrum | `output/verification/mskf_r/i.bin`                         |
| 输出kernels       | `output/verification/kernels/`                             |

---

## 参考文档

1. **Vitis HLS FFT参考**: `reference/vitis_hls_ftt的实现/interface_stream/`
2. **FFT Scaling修正**: `validation/board/FFT_SCALING_FIX_REPORT.md`
3. **命令规范**: `.github/copilot-instructions.md`
4. **Board验证**: `.github/skills/hls-board-validation/SKILL.md`