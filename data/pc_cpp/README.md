# CPU C++ 参考实现实验数据

本目录包含论文第五章中 CPU C++ 参考实现的实验数据。

## 数据文件说明

### 1. `platform_info.json`
平台配置信息。

**包含内容：**

#### 硬件配置
- **CPU**: Intel Xeon Platinum 8163 @ 2.50 GHz
  - 架构：2 × Socket，48 核心（96 线程）
  - 缓存：L1d 1.5 MiB, L1i 1.5 MiB, L2 48 MiB, L3 66 MiB
  - 频率范围：1.00 - 2.50 GHz
- **内存**: 93 GB DDR4
- **主板**: Inspur SA5212M5 (YZMB-00882-10E)

#### 软件配置
- 操作系统：Ubuntu 24.04 LTS
- 内核版本：Linux 6.8
- 编译器：GCC 13.3.0
- 优化选项：-O2
- C++ 标准：c++17
- 数据精度：单精度浮点
- 依赖库：FFTW 3.x, LAPACK, BLAS

### 2. `performance_data.json`
性能数据详细分解。

**包含内容：**
- 各模块执行时间（7个模块）
- 不同核数量下的性能数据（10/50/400核）
- 吞吐量计算

**关键指标：**
- TCC 直接成像：45.176 ms
- SOCS 10核成像：35.6 ms
- 吞吐量：28.09 fps

### 3. `accuracy_metrics.json`
精度指标原始数据。

**包含内容：**
- 最大绝对误差（MaxAE）
- 最大相对误差（MaxRE）
- 均方根误差（RMSE）
- 平均绝对误差（MeanAE）

### 4. `accuracy_analysis.json`
精度分析与对比。

**包含内容：**
- 不同核数量下的精度对比
- 与论文数据的对比分析
- 精度趋势分析

**关键发现：**
- 10核：RMSE = 5.474e-3
- 50核：RMSE = 9.273e-4（提升83.1%）
- 400核：RMSE = 2.570e-9（接近数值极限）

### 5. `performance_comparison.json`
性能对比分析。

**包含内容：**
- CPU C++ vs FPGA 性能对比
- CPU C++ vs MATLAB 性能对比
- 加速比分析
- 能效比分析

**关键对比：**
- FPGA vs CPU SOCS：3.37x 加速
- FPGA 能效比：67.4倍提升
- C++ vs MATLAB：8.06-10.6x 加速

### 6. `run_log_*.txt`
程序运行日志。

**包含内容：**
- 10核配置运行日志
- 50核配置运行日志
- 400核配置运行日志

### 7. `output_*/`
各配置的输出数据目录。

**包含内容：**
- `aerial_image_tcc_direct.bin`：TCC 直接成像结果
- `aerial_image_socs_kernel.bin`：SOCS 成像结果
- `tmpImgp_pad128.bin`：HLS Golden 数据
- `mskf_r.bin`, `mskf_i.bin`：掩模频谱
- `scales.bin`：特征值
- `kernels/`：SOCS 核数据

## 实验配置

### 光学参数
- 掩模尺寸：1024×1024
- 数值孔径（NA）：0.8
- 波长：193 nm
- 离焦量：0.2
- 光源类型：Annular
- 光源网格：101×101
- Annular 参数：inner=0.6, outer=0.9

### SOCS 核数量
- 10核：工程最优平衡点
- 50核：高精度应用
- 400核：理论验证（实际289核）

## 性能数据汇总

### 执行时间（毫秒）

| 模块 | 10核 | 50核 | 400核 |
|------|------|------|-------|
| Module 1 (Source) | 0.302 | 0.298 | 0.271 |
| Module 2 (Mask) | 36.972 | 36.252 | 32.962 |
| Module 3 (FFT Mask) | 78.420 | 76.025 | 74.836 |
| Module 4 (TCC) | 65.149 | 66.305 | 64.760 |
| Module 5 (TCC Image) | **45.176** | **46.74** | **46.068** |
| Module 6 (Kernels) | 48.836 | 62.656 | 118.672 |
| Module 7 (SOCS Image) | **35.6** | **41.548** | **69.327** |
| **总计** | **821.494** | **848.796** | **941.023** |

### 吞吐量（帧/秒）

| 核数量 | SOCS 成像时间 (ms) | 吞吐量 (fps) |
|--------|-------------------|-------------|
| 10核 | 35.6 | **28.09** |
| 50核 | 41.548 | 24.07 |
| 400核 | 69.327 | 14.42 |

## 精度数据汇总

| 核数量 | 最大绝对误差 | 最大相对误差 | RMSE |
|--------|-------------|-------------|------|
| 10核 | 1.047e-2 | 9.425e-1 | 5.474e-3 |
| 50核 | 1.323e-3 | 2.258e-1 | 9.273e-4 |
| 400核 | 2.980e-8 | 1.192e-7 | 2.570e-9 |

## 与 FPGA 对比

| 指标 | CPU C++ | FPGA | 加速比 |
|------|---------|------|--------|
| SOCS 10核成像时间 | 35.6 ms | 10.57 ms | **3.37x** |
| 吞吐量 | 28.09 fps | 94.6 fps | **3.37x** |
| 功耗 | 80 W | 4 W | - |
| 能效比 | 0.351 frames/J | 23.65 frames/J | **67.4x** |

## 数据来源

所有数据均来自：
- `validation/golden/src/litho.cpp` 程序运行结果
- 配置文件：`input/config/golden_1024*.json`
- 精度计算脚本：`data/pc_cpp/compute_accuracy.py`

## 使用示例

### Python 读取性能数据

```python
import json

# 读取性能数据
with open('performance_data.json', 'r') as f:
    perf_data = json.load(f)

# 打印 10 核配置的性能
perf_10k = perf_data['performance_breakdown']['10_kernels']
print(f"TCC Image: {perf_10k['module_5_tcc_image_us'] / 1000:.3f} ms")
print(f"SOCS Image: {perf_10k['module_7_socs_image_us'] / 1000:.3f} ms")
```

### Python 读取精度数据

```python
import json

# 读取精度数据
with open('accuracy_metrics.json', 'r') as f:
    acc_data = json.load(f)

# 打印 10 核配置的精度
acc_10k = acc_data['10_kernels']
print(f"Max Absolute Error: {acc_10k['max_absolute_error']:.6e}")
print(f"RMSE: {acc_10k['rmse']:.6e}")
```

## 更新记录

- **2026-05-06**: 初始版本，包含论文第五章所有 CPU C++ 实验数据

## 相关文档

- 论文第五章：`doc/论文/第五章.md`
- 源代码：`validation/golden/src/litho.cpp`
- 配置文件：`input/config/golden_1024*.json`
- FPGA 数据：`data/hls/`
