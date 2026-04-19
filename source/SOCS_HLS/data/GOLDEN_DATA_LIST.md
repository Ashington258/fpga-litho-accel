# HLS SOCS Golden Data Complete List

## 执行摘要

**生成时间**: 2026年4月7日
**配置参数**: Nx=4, Ny=4, NA=0.8, Lx=512, λ=193, σ_outer=0.9
**HLS目标**: 32×32 zero-padded IFFT → 17×17 tmpImgp输出

**状态**: ✅ 所有golden数据已生成并验证

---

## 1. 输入数据（HLS IP的输入）

### 1.1 Mask频域数据

**文件**: `mskf_r.bin`, `mskf_i.bin`
- **类型**: complex<float>
- **尺寸**: 512×512 = 262,144 complex elements
- **大小**: 1,048,576 bytes each (1 MB)
- **存储**: Real和Imag分离，交替对应complex数组
- **用途**: 已FFT的mask频域数据，HLS IP通过AXI-MM读取

**验证结果**:
```
mskf_r.bin: range [-0.017994, 0.061550]
mskf_i.bin: range [-0.000679, 0.000679]
```

### 1.2 特征值权重

**文件**: `scales.bin`
- **类型**: float
- **尺寸**: nk = 10 eigenvalues
- **大小**: 40 bytes
- **用途**: SOCS分解的特征值权重，用于强度累加

**数值**:
```
[5.7594705, 2.4737134, 2.4737134, 0.74804795, 0.6459604, 
 0.29871237, 0.29871237, 0.16575447, 0.14633182, 0.08639868]
```

### 1.3 SOCS频域核

**文件**: `kernels/krn_k_r.bin`, `kernels/krn_k_i.bin` (k=0..9)
- **类型**: complex<float>
- **尺寸**: 每个 9×9 = 81 complex elements
- **大小**: 324 bytes each
- **总数**: 10个核
- **用途**: SOCS分解的频域核，用于频域点乘

**验证结果**:
```
Kernel 0: range [-0.000000, 0.386092] (largest eigenvalue)
Kernel 1: range [-0.218963, 0.218963]
Kernel 2: range [-0.215947, 0.215947]
...
```

---

## 2. 中间输出（HLS IP的直接输出）⭐

### 2.1 tmpImgp_pad32.bin - HLS唯一Golden

**文件**: `tmpImgp_pad32.bin`
- **类型**: float
- **尺寸**: 17×17 = 289 elements
- **大小**: 1,156 bytes
- **用途**: **HLS IP的直接输出目标**
- **来源**: calcSOCS步骤1-5（频域点乘 → 32×32 IFFT → 强度累加 → fftshift → 提取中心17×17）

**关键特性**:
- ✅ 这是HLS IP应该输出的结果
- ✅ 不包含Fourier Interpolation步骤
- ✅ 从32×32 tmpImgp中提取的中心17×17区域

**验证结果**:
```
Range: [0.002412, 0.146997]
Mean: 0.044934, StdDev: 0.038807
```

**HLS验证标准**:
```cpp
// HLS testbench
float golden_tmpImgp[289];
load_golden("tmpImgp_pad32.bin", golden_tmpImgp);

float hls_tmpImgp[289];
socs_top(mskf, krns, scales, hls_tmpImgp, ...);

// 验收标准
max_abs_error < 1e-5
normalized_rmse < 1e-4
```

---

## 3. 最终输出（端到端验证）

### 3.1 SOCS方法完整输出

**文件**: `aerial_image_socs_kernel.bin`
- **类型**: float
- **尺寸**: 512×512 = 262,144 elements
- **大小**: 1,048,576 bytes (1 MB)
- **流程**: tmpImgp_pad32 → Fourier Interpolation → crop to 512×512
- **用途**: 端到端验证，评估完整系统正确性

**验证结果**:
```
Mean: 0.014632, StdDev: 0.028109
```

### 3.2 TCC直接计算输出

**文件**: `aerial_image_tcc_direct.bin`
- **类型**: float
- **尺寸**: 512×512 = 262,144 elements
- **大小**: 1,048,576 bytes (1 MB)
- **流程**: TCC直接计算（不使用SOCS近似）
- **用途**: 理论标准，评估SOCS近似误差

**验证结果**:
```
Mean: 0.015253, StdDev: 0.027946
```

### 3.3 一致性验证

**SOCS vs TCC相对差异**: 4.07%
```
rel_diff = |SOCS_mean - TCC_mean| / TCC_mean = 0.0407
```

**结论**: ✅ 在nk=10截断误差范围内（< 10%），验证通过

---

## 4. 辅助数据（调试参考）

### 4.1 TCC矩阵

**文件**: `tcc_r.bin`, `tcc_i.bin`
- **类型**: complex<float>
- **尺寸**: 81×81 = 6,561 complex elements
- **大小**: 26,244 bytes each
- **用途**: TCC矩阵，用于理解SOCS核来源

### 4.2 可视化文件

**图像文件**:
- `source.png`: 光源分布
- `mask.png`: Mask图案
- `aerial_image_tcc_direct.png`: TCC直接成像
- `aerial_image_socs_kernel.png`: SOCS重建成像
- `aerial_image_socs_threshold.png`: 阈值分割结果

**元数据**:
- `kernels/kernel_info.txt`: 核信息和特征值列表
- `fft_meta.txt`: FFT相关元数据

---

## 5. 数据格式规范

### 5.1 存储格式

| 数据类型 | 格式 | 字节序 | 存储 |
|----------|------|--------|------|
| float | IEEE 754 single | Little-endian | Binary, row-major |
| complex | [real, imag] pairs | Little-endian | Binary, interleaved |

### 5.2 尺寸约定

| 符号 | 当前值 | 说明 |
|------|--------|------|
| Lx, Ly | 512 | Mask物理尺寸 |
| Nx, Ny | 4 | 动态计算参数 |
| convX, convY | 17 | 物理卷积输出 (4×Nx+1) |
| fftConvX, fftConvY | 32 | IFFT执行尺寸 (nextPowerOfTwo) |
| kerX, kerY | 9 | SOCS核尺寸 (2×Nx+1) |
| nk | 10 | SOCS核数量 |

---

## 6. HLS验证流程

### 6.1 C Simulation

**输入**: mskf, scales, kernels (从golden加载)
**输出**: hls_tmpImgp[289]
**对比**: tmpImgp_pad32.bin
**标准**: max_error < 1e-5

### 6.2 C Synthesis

**目标**: 
- Fmax ≥ 270 MHz
- Latency < 500k cycles (方案A, Nx=4)
- 资源利用率合理

### 6.3 Co-Simulation

**验证**: RTL与C代码行为一致
**对比**: 使用相同的golden数据

### 6.4 Package

**输出**: RTL或Kernel包
**格式**: rtl (推荐) 或 kernel

---

## 7. 文件路径映射

### 7.1 Verification输出（原始位置）

```
/root/project/FPGA-Litho/output/verification/
├── mskf_r.bin                    # 输入：Mask频域实部
├── mskf_i.bin                    # 输入：Mask频域虚部
├── scales.bin                    # 输入：特征值权重
├── tmpImgp_pad32.bin            # ⭐ HLS Golden输出
├── aerial_image_socs_kernel.bin  # 输出：SOCS完整结果
├── aerial_image_tcc_direct.bin   # 参考：TCC理论结果
├── kernels/
│   ├── kernel_info.txt
│   ├── krn_0_r.bin, krn_0_i.bin  # 输入：SOCS核0
│   ├── krn_1_r.bin, krn_1_i.bin  # 输入：SOCS核1
│   └── ...                        # 共10个核
└── *.png                         # 可视化文件
```

### 7.2 HLS项目数据（复制目标）

```
/root/project/FPGA-Litho/source/SOCS_HLS/data/
├── mskf_r.bin                    # 从verification复制
├── mskf_i.bin
├── scales.bin
├── tmpImgp_pad32.bin            # ⭐ HLS Golden
├── aerial_image_socs_kernel.bin  # 端到端参考
└── kernels/
    ├── krn_0_r.bin, krn_0_i.bin
    ├── krn_1_r.bin, krn_1_i.bin
    └── ...
```

---

## 8. 验证检查清单

### 8.1 文件完整性

- [x] mskf_r.bin, mskf_i.bin 存在且大小正确
- [x] scales.bin 存在且包含10个特征值
- [x] 10个SOCS核文件存在且大小正确
- [x] **tmpImgp_pad32.bin 存在且大小为289 floats**
- [x] aerial_image_socs_kernel.bin 存在且大小正确
- [x] aerial_image_tcc_direct.bin 存在且大小正确

### 8.2 数值正确性

- [x] mskf数值范围合理
- [x] 特征值按降序排列（符合eigendecomposition特性）
- [x] **tmpImgp数值范围合理（非全零或异常值）**
- [x] SOCS与TCC相对差异 < 10%

### 8.3 算法一致性

- [x] litho.cpp已使用nextPowerOfTwo自动填充
- [x] 当前配置Nx=4，IFFT尺寸32×32
- [x] 输出尺寸17×17符合预期
- [x] 补零布局使用dif偏移（litho.cpp默认）

---

## 9. 关键发现与决策

### 9.1 算法确认

✅ **litho.cpp已满足HLS要求**
- 使用nextPowerOfTwo()自动填充到32×32
- 无需额外算法改写
- 可直接使用litho.cpp作为参考实现

### 9.2 配置确认

✅ **当前配置适用于方案A**
- Nx=4（非Nx=16）
- IFFT尺寸32×32（非128×128）
- 输出17×17（非65×65）

### 9.3 补零布局

**当前使用**: dif偏移（bottom-right embedding）
```cpp
int difX = fftConvX - kerX;  // = 32 - 9 = 23
int by = difY + ky;  // Embed at bottom-right
```

**建议评估**: centered embedding（相位对齐可能更好）
```cpp
int offX = (fftConvX - kerX) / 2;  // = 11
int by = offY + ky;  // Embed at center
```

---

## 10. 后续行动

### 10.1 立即可执行

1. **复制golden到HLS项目**:
   ```bash
   cd /root/project/FPGA-Litho
   mkdir -p source/SOCS_HLS/data/kernels
   cp output/verification/mskf_*.bin source/SOCS_HLS/data/
   cp output/verification/scales.bin source/SOCS_HLS/data/
   cp output/verification/tmpImgp_pad32.bin source/SOCS_HLS/data/
   cp output/verification/kernels/*.bin source/SOCS_HLS/data/kernels/
   ```

2. **开始HLS开发**:
   - 阶段1: 验证32点FFT
   - 阶段2: 实现32×32 2D IFFT
   - 阶段3: 完整calcSOCS实现

### 10.2 验证流程

调用`hls-full-validation` skill执行完整验证：
- C Simulation: 使用tmpImgp_pad32.bin验证
- C Synthesis: 检查性能指标
- Co-Simulation: RTL行为验证
- Package: 导出IP

---

## 11. 参考文档

- **详细计划**: `source/SOCS_HLS/data/GOLDEN_EXTRACTION_PLAN.md`
- **提取总结**: `source/SOCS_HLS/data/GOLDEN_EXTRACTION_SUMMARY.md`
- **任务清单**: `source/SOCS_HLS/SOCS_TODO.md`
- **算法参考**: `validation/golden/src/litho.cpp`
- **配置参数**: `input/config/config.json`

---

**文档生成时间**: 2026年4月7日
**验证状态**: ✅ 所有golden数据已生成并验证通过
**下一步**: 复制golden到HLS项目，开始HLS开发
