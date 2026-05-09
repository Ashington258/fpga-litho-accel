# 批量测试总结报告

**生成时间**: 2026-05-09 12:20

---

## 测试套件概览

### 1. Different_mask_tests（工业Mask测试）

**测试目的**: 验证10个ICCAD 2013基准测试mask的光刻成像效果

**配置参数**:
- 光学参数: NA=0.8, wavelength=193nm, defocus=0.2
- Kernel数量: nk=10
- Mask尺寸: 1024×1024
- FFT尺寸: 1024×1024

**测试结果**:
- ✅ 总测试数: 10
- ✅ 成功: 10
- ❌ 失败: 0
- 📊 成功率: **100.0%**
- ⏱️ 总耗时: 8.31秒

**测试用例详情**:

| 测试名称 | 描述 | 状态 | 耗时(秒) | 输出目录 |
|---------|------|------|---------|---------|
| T1 | ICCAD 2013 benchmark mask T1 | ✅ success | 0.85 | output/Different_mask_tests/T1 |
| T2 | ICCAD 2013 benchmark mask T2 | ✅ success | 0.82 | output/Different_mask_tests/T2 |
| T3 | ICCAD 2013 benchmark mask T3 | ✅ success | 0.83 | output/Different_mask_tests/T3 |
| T4 | ICCAD 2013 benchmark mask T4 | ✅ success | 0.80 | output/Different_mask_tests/T4 |
| T5 | ICCAD 2013 benchmark mask T5 | ✅ success | 0.88 | output/Different_mask_tests/T5 |
| T6 | ICCAD 2013 benchmark mask T6 | ✅ success | 0.86 | output/Different_mask_tests/T6 |
| T7 | ICCAD 2013 benchmark mask T7 | ✅ success | 0.84 | output/Different_mask_tests/T7 |
| T8 | ICCAD 2013 benchmark mask T8 | ✅ success | 0.80 | output/Different_mask_tests/T8 |
| T9 | ICCAD 2013 benchmark mask T9 | ✅ success | 0.88 | output/Different_mask_tests/T9 |
| T10 | ICCAD 2013 benchmark mask T10 | ✅ success | 0.77 | output/Different_mask_tests/T10 |

**输出文件**（每个测试用例）:
- `aerial_image_tcc_direct.bin` - TCC直接成像结果（4MB）
- `aerial_image_socs_kernel.bin` - SOCS kernel重建成像结果（4MB）
- `aerial_image_*.png` - 可视化图像
- `mskf_r.bin`, `mskf_i.bin` - Mask频域数据（各4MB）
- `scales.bin` - SOCS特征值（40B）
- `kernels/` - SOCS kernel数据
- `tmpImgp_full_128.bin` - 中间结果（64KB）

---

### 2. Different_resolution_tests（多分辨率测试）

**测试目的**: 验证不同分辨率下的光刻成像效果和Nx计算正确性

**配置参数**:
- 光学参数: NA=0.8, wavelength=193nm, defocus=0.2
- Kernel数量: nk=10
- 分辨率范围: 256×256 到 2048×2048

**测试结果**:
- ✅ 总测试数: 4
- ✅ 成功: 4
- ❌ 失败: 0
- 📊 成功率: **100.0%**
- ⏱️ 总耗时: 6.59秒

**测试用例详情**:

| 测试名称 | 描述 | 分辨率 | 预期Nx | 状态 | 耗时(秒) | 输出目录 |
|---------|------|--------|--------|------|---------|---------|
| 256x256 | Low resolution test | 256×256 | 2 | ✅ success | 0.07 | output/Different_resolution_tests/256x256 |
| 512x512 | Medium resolution test | 512×512 | 4 | ✅ success | 0.23 | output/Different_resolution_tests/512x512 |
| 1024x1024 | High resolution test | 1024×1024 | 8 | ✅ success | 0.86 | output/Different_resolution_tests/1024x1024 |
| 2048x2048 | Ultra-high resolution test | 2048×2048 | 16 | ✅ success | 5.43 | output/Different_resolution_tests/2048x2048 |

**Nx计算验证**:
- 公式: $N_x = \lfloor \frac{NA \times L_x \times (1 + \sigma_{outer})}{\lambda} \rfloor$
- 参数: NA=0.8, σ_outer=0.9, λ=193nm
- 验证结果: 所有分辨率下的Nx计算均正确

**性能分析**:
- 256×256: 0.07秒（基准）
- 512×512: 0.23秒（3.3倍）
- 1024×1024: 0.86秒（12.3倍）
- 2048×2048: 5.43秒（77.6倍）
- **复杂度**: 接近 $O(N^2)$（FFT主导）

---

## 总体统计

| 指标 | 数值 |
|-----|------|
| 总测试套件数 | 2 |
| 总测试用例数 | 14 |
| 成功用例数 | 14 |
| 失败用例数 | 0 |
| **总体成功率** | **100.0%** |
| 总执行时间 | 14.90秒 |

---

## 关键发现

### 1. 配置文件路径处理
- **问题**: litho.cpp 通过配置文件路径推导项目根目录
- **解决方案**: 临时配置文件必须放在 `input/config/` 下
- **经验**: 避免在配置文件中使用绝对路径

### 2. 性能特征
- **FFT主导**: 计算复杂度接近 $O(N^2)$
- **分辨率影响**: 2048×2048 比 256×256 慢约78倍
- **优化方向**: 考虑使用HLS FFT IP加速

### 3. 输出文件组织
- **独立子目录**: 每个测试用例有独立输出目录
- **完整数据**: 包含中间结果和最终成像
- **可视化**: 自动生成PNG图像便于分析

---

## 后续工作建议

1. **数据分析**: 对比不同mask的成像质量差异
2. **性能优化**: 针对高分辨率场景优化FFT计算
3. **自动化验证**: 添加RMSE自动对比功能
4. **可视化增强**: 生成对比图表和统计报告

---

## 文件位置

- **测试报告**: 
  - `output/Different_mask_tests/test_report.json`
  - `output/Different_resolution_tests/test_report.json`
- **执行日志**:
  - `output/Different_mask_tests/execution.log`
  - `output/Different_resolution_tests/execution.log`
- **配置文件**:
  - `input/config/Different_mask_tests.json`
  - `input/config/Different_resolution_tests.json`
- **批量测试脚本**: `validation/batch_test_runner.py`
