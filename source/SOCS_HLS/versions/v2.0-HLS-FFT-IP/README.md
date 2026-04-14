# v2.0-HLS-FFT-IP - HLS FFT IP Implementation

## ✅ 推荐方案

此版本使用 Vitis HLS 内置 `hls::fft` IP，预计可大幅降低资源使用量。

---

## 预期优势

| 对比项 | v1.0 DFT 直接法 | v2.0 HLS FFT IP |
|--------|----------------|-----------------|
| DSP 使用 | 8,080 (590%) ❌ | ~50-100 (<10%) ✅ |
| LUT 使用 | 647K (398%) ❌ | ~50K (30%) ✅ |
| Latency | 157,863 cycles | ~4,000 cycles ✅ |
| II | 8 (resource limit) | 1 (pipeline) ✅ |
| 复杂度 | O(N²) | O(N log N) ✅ |

---

## 实现状态

| 步骤 | 状态 | 备注 |
|------|------|------|
| 基础代码 | ✅ 已存在 | `src/socs_fft.cpp` 已有 hls::fft 调用 |
| C Simulation | 🔄 待运行 | 需修复配置问题后验证 |
| C Synthesis | ❌ 未执行 | 待 C Sim 成功后执行 |
| Co-Simulation | ❌ 未执行 | 待 Synth 成功后执行 |
| Board Validation | ❌ 未执行 | 待 CoSim 成功后执行 |

---

## 技术细节

### FFT 配置（参考 vitis_hls_fft）
```cpp
const int FFT_LENGTH = 32;
const char FFT_NFFT_MAX = 5;  // log2(32)
const bool FFT_USE_FLT_PT = 1; // Floating-point mode
const char FFT_INPUT_WIDTH = 32;
const char FFT_OUTPUT_WIDTH = 32;

// Architecture
const hls::ip_fft::arch FFT_ARCH = hls::ip_fft::pipelined_streaming_io;
const hls::ip_fft::ordering FFT_OUTPUT_ORDER = hls::ip_fft::natural_order;
```

### API 选择
- **Array-based API**: `hls::fft<config>(fft_in, fft_out, &status, &config)`
  - 适合简单场景，直接数组输入输出
- **Stream-based API**: `hls::fft<config>(in_stream, out_stream, fwd_inv, scale, ...)`
  - 适合流水线架构，DATAFLOW 设计

当前 `socs_fft.cpp` 使用 **Array-based API**。

---

## 开发任务

1. **修复 FFT 配置问题**
   - 检查 `ARRAY_PARTITION` 是否导致端口不匹配
   - 验证 fixed-point vs floating-point 模式选择

2. **创建 HLS 配置文件**
   - 文件名：`script/config/hls_config_socs_fft.cfg`
   - 参考：`reference/vitis_hls_ftt的实现/interface_stream/hls_config.cfg`

3. **创建测试 Bench**
   - 文件名：`tb/tb_socs_fft.cpp`
   - 验证：数值精度（RMSE vs golden data）

4. **运行完整验证流程**
   - C Simulation → C Synthesis → Co-Simulation

---

## 文件结构

```
source/SOCS_HLS/
├── src/
│   ├── socs_fft.cpp    # HLS FFT IP 实现（已存在）
│   └── socs_fft.h      # FFT 配置头文件（已存在）
├── script/config/
│   └── hls_config_socs_fft.cfg  # 待创建
├── tb/
│   └── tb_socs_fft.cpp           # 待创建
└── socs_fft_comp/               # 待创建（综合输出）
```

---

## 下一步行动

```bash
# 1. 检查现有 socs_fft.cpp 代码
cd e:\fpga-litho-accel\source\SOCS_HLS

# 2. 创建 HLS 配置文件
# （参考 reference/vitis_hls_ftt的实现/interface_stream/hls_config.cfg）

# 3. 运行 C Simulation
vitis-run --mode hls --csim --config script/config/hls_config_socs_fft.cfg --work_dir socs_fft_csim

# 4. 检查 RMSE 结果
# 目标：< 1e-5（与 DFT 版本类似精度）
```

---

*创建日期：2026-04-15*
*状态：开发中*