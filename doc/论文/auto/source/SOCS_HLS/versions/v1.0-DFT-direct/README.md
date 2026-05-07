# v1.0-DFT-direct - Direct DFT Implementation

## ⚠️ IMPORTANT WARNING

**此版本功能正确但不可部署！资源利用率严重超过目标器件容量。**

- **DSP 使用率**: 590% (需要 8,080 DSP，器件只有 1,360)
- **LUT 使用率**: 398% (需要 647K LUT，器件只有 162K)
- **BRAM 使用率**: 189% (需要 1,366 BRAM，器件只有 720)

**用途**: 仅用于算法功能验证，不适合实际 FPGA 部署。

---

## 验证结果

### C Simulation（成功）
```
RMSE: 1.02e-07
Error rate: 0%
Max error: 1.4e-07
```

✅ 算法实现与 CPU reference 完全一致（FFTW BACKWARD convention）

### C Synthesis（成功但资源超限）
```
Latency: 157,863 cycles
Interval: 157,864 cycles
Timing: Target 5ns, Slack 0.00ns
```

⚠️ II Violation: `fft_2d_rows` 目标 II=1，实际 II=8（Resource Limitation）

---

## 算法实现

### 直接 DFT 公式
- **FFT**: $X[k] = \sum_{n=0}^{N-1} x[n] e^{-2\pi i n k / N}$
- **IFFT (FFTW BACKWARD)**: $x[n] = \sum_{k=0}^{N-1} X[k] e^{+2\pi i n k / N}$

### 关键特性
- 无 FFT IP 依赖，使用 `sin()`/`cos()` 计算
- 64 个并行 `sin_or_cos_double_s` 模块实例化
- O(N²) 复杂度：32×32 点需要 1024 次复数乘法/点

---

## 文件清单

| 文件                                             | 说明             |
| ------------------------------------------------ | ---------------- |
| `../../src/socs_simple.cpp`                      | DFT 直接计算源码 |
| `../../src/socs_simple.h`                        | 头文件           |
| `../../socs_simple_synth/`                       | C 综合输出目录   |
| `../../script/config/hls_config_socs_simple.cfg` | HLS 配置文件     |

---

## 性能分析

### 资源瓶颈分析
```
Top 5 Resource Consumers:
1. sin_or_cos_double_s (64 instances): 5,120 DSP
2. fft_2d_rows: 2,400 DSP
3. transpose_2d: 1,560 DSP
4. Complex multiplication: 1,000 DSP
Total: ~8,080 DSP
```

### 改进方向
切换到 HLS FFT IP：
- FFT IP 使用 Radix-2/4 Butterfly 结构
- DSP 使用量从 8,080 降至 ~50-100
- Latency 从 157,863 降至 ~4,000 cycles

---

## 使用指南

### 运行 C Simulation
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_socs_simple.cfg --work_dir socs_simple_csim
```

### 查看 C Synthesis 报告
```bash
# 报告路径
source/SOCS_HLS/socs_simple_synth/hls/syn/report/csynth.rpt
```

---

*创建日期：2026-04-15*