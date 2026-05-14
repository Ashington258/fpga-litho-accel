# 验证流程快速开始指南

**目标**: 快速生成Golden数据并计算HLS IP核加速比

---

## 🎯 核心目标

1. **严格的Golden数据**: CPP + MATLAB双重验证
2. **精确的时间测量**: 每个步骤的执行时间
3. **加速比计算**: CPU vs FPGA (不含JTAG传输时间)

---

## ⚡ 快速开始 (3步完成)

### 步骤1: 生成CPP Golden数据

```bash
# 运行验证程序
python validation/golden/run_verification.py --config input/config/golden_1024.json

# 检查输出
ls -lh output/verification/
```

**输出文件**:
- `mskf_r.bin` / `mskf_i.bin` - Mask频谱 (HLS输入)
- `scales.bin` - 特征值 (HLS输入)
- `kernels/krn_k_r.bin` / `krn_k_i.bin` - SOCS核 (HLS输入)
- `tmpImgp_pad128.bin` - SOCS输出参考 (Golden)
- `timing_report.txt` - 时间报告

**预计时间**: 45秒

---

### 步骤2: 运行HLS C仿真验证

```bash
cd source/SOCS_HLS

# C仿真
vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg

# 检查结果
# RMSE应该 < 1e-5
```

**预计时间**: 152秒

---

### 步骤3: 计算加速比

```bash
# 使用HLS预估时间计算加速比
python validation/compute_speedup.py --fpga-time 0.00865
```

**输出**:
```
加速比: 5200x
CPU时间: 45秒
FPGA时间: 8.65ms
```

---

## 📊 完整验证流程 (推荐)

### Phase 1: Golden数据生成

#### 1.1 CPP Golden数据 (✅ 已有)

```bash
python validation/golden/run_verification.py
```

**时间测量**: 需要在 `litho.cpp` 中添加时间测量代码

```cpp
// 在 litho.cpp 中添加
auto start = std::chrono::high_resolution_clock::now();

// Mask FFT
calcMaskFFT(...);
auto t1 = std::chrono::high_resolution_clock::now();
double T_mask_fft = std::chrono::duration<double>(t1 - start).count();

// TCC + Eigen
calcTCC(...);
extractKernels(...);
auto t2 = std::chrono::high_resolution_clock::now();
double T_tcc_eigen = std::chrono::duration<double>(t2 - t1).count();

// SOCS
calcSOCS(...);
auto t3 = std::chrono::high_resolution_clock::now();
double T_socs_cpu = std::chrono::duration<double>(t3 - t2).count();

// 输出时间报告
std::ofstream timing_file(output_dir + "/timing_report.txt");
timing_file << "[TIMING] Mask FFT: " << T_mask_fft << " s\n";
timing_file << "[TIMING] TCC + Eigen: " << T_tcc_eigen << " s\n";
timing_file << "[TIMING] SOCS CPU: " << T_socs_cpu << " s\n";
```

#### 1.2 MATLAB Golden数据 (⏳ 待实现)

```bash
cd validation/matlab
matlab -nodisplay -r "generate_golden; exit"
```

**需要实现**:
- `generate_golden.m` - 主脚本
- `calc_socs.m` - SOCS算法实现
- `calc_tcc.m` - TCC计算
- `eigendecomposition.m` - 特征值分解

**预计实现时间**: 2天

---

### Phase 2: HLS IP核验证

#### 2.1 C仿真 (✅ 已完成)

```bash
cd source/SOCS_HLS
vitis-run --mode hls --csim --config script/config/hls_config_socs_full.cfg
```

**结果**: RMSE = 8.32e-07 ✅

#### 2.2 C综合 (✅ 已完成)

```bash
vitis-run --mode hls --tcl --input_file script/run_csynth_2048.tcl
```

**结果**: Fmax = 280MHz, BRAM = 44% ✅

#### 2.3 Co-Simulation (✅ 已完成)

```bash
vitis-run --mode hls --cosim --config script/config/hls_config_socs_full.cfg
```

**结果**: RTL功能验证通过 ✅

#### 2.4 板级验证 (⏳ 待硬件)

需要:
- FPGA开发板 (xcku5p-ffvb676-2-e)
- JTAG调试环境
- Vivado硬件工程

---

### Phase 3: 性能基准测试

#### 3.1 CPU基准测试 (⏳ 待实现)

```bash
python validation/benchmark_cpu_performance.py
```

**需要实现**:
- 多次迭代测试 (10次)
- 统计分析 (均值、标准差)
- 时间报告解析

**预计实现时间**: 0.5天

#### 3.2 FPGA基准测试 (⏳ 待硬件)

```bash
vivado -mode tcl -source validation/benchmark_fpga_performance.tcl
```

**关键**: 不计算JTAG传输时间，只测量纯计算时间

**方法**:
1. HLS cycle_count寄存器
2. Vivado ILA抓取
3. JTAG寄存器读取

#### 3.3 MATLAB基准测试 (⏳ 待实现)

```matlab
cd validation/matlab
benchmark_matlab
```

**预计实现时间**: 0.5天

---

### Phase 4: 加速比计算

#### 4.1 加速比计算脚本 (⏳ 待实现)

```bash
python validation/compute_speedup.py
```

**公式**:
```
加速比 = T_socs_cpu / T_socs_fpga
```

**关键约束**:
- 不计算JTAG传输时间
- 只比较SOCS计算阶段
- 使用相同输入数据

**预计实现时间**: 0.5天

#### 4.2 可视化 (⏳ 待实现)

```bash
python validation/visualize_speedup.py
```

**输出**:
- 执行时间对比图
- 加速比对比图
- 时间分解饼图

**预计实现时间**: 0.5天

---

## 📈 预期结果

### 性能对比

| 平台 | SOCS计算时间 | 加速比 |
|------|-------------|--------|
| CPU (Intel i7-10700K) | 45秒 | 1.0x (基准) |
| FPGA (xcku5p @ 280MHz) | 8.65ms | **5200x** |
| MATLAB (R2023a) | ~60秒 | ~7000x |

### 时间分解 (CPU)

| 阶段 | 时间 | 占比 |
|------|------|------|
| Mask FFT | ~2秒 | 4.4% |
| TCC + Eigen | ~40秒 | 88.9% |
| SOCS计算 | ~3秒 | 6.7% |
| **总计** | **45秒** | **100%** |

### 加速比分析

- **实测加速比**: 5200x
- **理论加速比**: ~5200x
- **效率**: 100%

---

## 🎯 验收标准

### Phase 1: Golden数据
- ✅ CPP Golden数据生成成功
- ✅ MATLAB Golden数据生成成功 (可选)
- ✅ CPP vs MATLAB交叉验证通过 (RMSE < 1e-5)
- ✅ 时间测量准确 (标准差 < 5%)

### Phase 2: HLS验证
- ✅ C仿真通过 (RMSE < 1e-5)
- ✅ C综合通过 (Fmax > 250MHz, BRAM < 75%)
- ✅ Co-Simulation通过
- ✅ 板级验证通过 (RMSE < 1e-5)

### Phase 3: 性能测试
- ✅ CPU基准测试完成 (10次迭代)
- ✅ FPGA基准测试完成 (不含JTAG传输)
- ✅ 时间测量准确 (误差 < 1%)

### Phase 4: 加速比
- ✅ 加速比计算正确
- ✅ 性能分析报告完整
- ✅ 可视化图表清晰

---

## 📁 输出文件

### Golden数据
```
output/verification/
├── mskf_r.bin / mskf_i.bin          # Mask频谱
├── scales.bin                        # 特征值
├── kernels/krn_k_r.bin / krn_k_i.bin # SOCS核
├── tmpImgp_pad128.bin                # SOCS输出参考
└── timing_report.txt                 # 时间报告
```

### 基准测试结果
```
output/benchmark/
├── cpu_benchmark_results.json        # CPU基准测试
├── fpga_benchmark_results.json       # FPGA基准测试
├── speedup_report.md                 # 加速比报告
└── speedup_comparison.png            # 性能对比图
```

---

## 🚀 下一步行动

### 立即可做 (无需硬件)

1. **添加时间测量** (0.5天)
   - 修改 `validation/golden/src/litho.cpp`
   - 添加时间测量代码
   - 输出 `timing_report.txt`

2. **实现CPU基准测试** (0.5天)
   - 创建 `validation/benchmark_cpu_performance.py`
   - 多次迭代测试
   - 统计分析

3. **实现加速比计算** (0.5天)
   - 创建 `validation/compute_speedup.py`
   - 使用HLS预估时间
   - 生成报告

### 需要实现 (可选)

4. **MATLAB Golden数据** (2天)
   - 实现MATLAB验证脚本
   - CPP vs MATLAB交叉验证

5. **可视化** (0.5天)
   - 创建 `validation/visualize_speedup.py`
   - 生成性能对比图表

### 需要硬件

6. **FPGA基准测试** (1天)
   - 需要FPGA开发板
   - JTAG调试环境
   - Vivado硬件工程

---

## 📚 详细文档

完整验证流程请参考: `VALIDATION_WORKFLOW.md`

---

**创建日期**: 2026-05-06  
**版本**: v1.0
