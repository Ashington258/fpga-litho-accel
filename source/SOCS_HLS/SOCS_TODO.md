# SOCS HLS 任务清单

**最后更新**: 2025-01-21
**当前状态**: Phase 0 完成 (基础架构整理)
**下一步**: Phase 1 - HLS综合与优化

---

## Phase 0: 源代码整理 ✅ (已完成)

### 任务 0.1: 文件结构整理 ✅
- **目标**: 统一HLS源代码，删除冗余文件
- **完成内容**:
  - ✅ 创建 `socs_config.h` - 动态配置系统
    - Nx/Ny动态计算公式: `Nx = floor(NA * Lx * (1+σ_outer) / λ)`
    - FFT尺寸动态推导 (32/64/128)
    - 固定点FFT配置 (Q1.31格式)
    - Vitis FFT IP配置结构体
  
  - ✅ 更新 `socs_simple.cpp` - 统一实现
    - 使用 `socs_config.h` 配置
    - 删除硬编码Nx值
    - 保持Vitis FFT IP集成
    - DSP优化: 8064 → ~400
  
  - ✅ 更新 `socs_simple.h` - 统一头文件
  
  - ✅ 删除冗余文件:
    - `socs_hls.cpp` (Nx=16版本)
    - `socs_fft.cpp` (128×128辅助函数)
    - `socs_fft.h` (FFT配置)
  
  - ✅ 最终文件结构:
    ```
    source/SOCS_HLS/src/
      ├── socs_config.h    (动态配置)
      ├── socs_simple.cpp  (主实现)
      └── socs_simple.h    (头文件)
    ```

### 任务 0.2: 配置系统验证 ⏳ (待验证)
- **目标**: 验证动态配置编译正确性
- **内容**:
  - ⏳ C Simulation验证 (socs_simple)
  - ⏳ 验证FFT IP正确集成
  - ⏳ 验证DSP消耗降低

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

| 参数   | 默认值 | 计算公式                     | 说明              |
|--------|--------|------------------------------|-------------------|
| Nx     | 4      | floor(NA*Lx*(1+σ)/λ)         | 频域截断宽度      |
| convX  | 17     | 4*Nx + 1                     | 卷积输出宽度      |
| fftConvX | 32   | nextPow2(convX)              | FFT网格尺寸       |
| kerX   | 9      | 2*Nx + 1                     | Kernel支持宽度    |

### FFT实现对比

| 方案           | DSP消耗 | 验证状态  | 说明                |
|----------------|---------|-----------|---------------------|
| 直接DFT        | 8,064   | ❌ 超限    | 旧socs_fft.cpp      |
| HLS FFT IP     | ~400    | ✅ 待验证  | 当前socs_simple.cpp |
| LUT-only FFT   | ~0      | ⏳ 可选    | DSP-free方案        |

### 资源利用率目标 (xcku5p-ffvb676-2-e)

| 资源 | 可用量 | 目标上限 | 当前使用 | 状态   |
|------|--------|----------|----------|--------|
| BRAM | 960    | ≤960     | ?        | ⏳     |
| DSP  | 1,824  | ≤500     | 16       | ✅     |
| FF   | 433,920| ≤200k    | ?        | ⏳     |
| LUT  | 216,960| ≤150k    | ?        | ⏳     |

---

## 关键文件路径

| 文件类型         | 路径                                        |
|------------------|---------------------------------------------|
| HLS源码          | `source/SOCS_HLS/src/socs_simple.cpp`      |
| 配置头文件       | `source/SOCS_HLS/src/socs_config.h`        |
| Testbench        | `source/SOCS_HLS/tb/tb_socs_simple.cpp`    |
| HLS Config       | `source/SOCS_HLS/script/config/hls_config_socs_simple.cfg` |
| Golden数据       | `output/verification/`                     |
| 输出mask spectrum| `output/verification/mskf_r/i.bin`         |
| 输出kernels      | `output/verification/kernels/`             |

---

## 参考文档

1. **Vitis HLS FFT参考**: `reference/vitis_hls_ftt的实现/interface_stream/`
2. **FFT Scaling修正**: `validation/board/FFT_SCALING_FIX_REPORT.md`
3. **命令规范**: `.github/copilot-instructions.md`
4. **Board验证**: `.github/skills/hls-board-validation/SKILL.md`