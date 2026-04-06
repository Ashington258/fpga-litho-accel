# Litho-TCC-HLS 简化参考版本

> **用途**: 为HLS重构提供固定数组+固定循环边界的参考实现
> **创建日期**: 2026-04-06
> **状态**: 基础框架完成

---

## 目的

原始代码 (`klitho_tcc.cpp`) 使用动态内存和动态循环边界，不符合HLS要求：
- `vector<>` → HLS不支持
- 动态计算的循环边界 (`floor((-1 - sx) * Lx_norm)`) → HLS需要固定边界
- 动态索引 (`tcc[ID1 * tccSize + ID2]`) → HLS需要固定数组

本简化参考版本展示如何改造：
1. **固定数组尺寸** → 通过模板参数化
2. **固定循环边界** → 遍历固定范围 + 内部条件判断
3. **HLS数学函数** → `hls::sqrt`, `hls::cos`, `hls::sin`

---

## 文件结构

```
Litho-TCC-HLS/source/
├── hls_ref_types.h      ← 固定数组类型定义（模板参数化）
├── hls_ref_pupil.cpp    ← Pupil Function计算（固定循环边界示例）
├── hls_ref_tcc.cpp      ← TCC矩阵计算（计划）
├── hls_ref_fft.cpp      ← FFT封装（计划）
└── hls_ref_image.cpp    ← calcImage实现（计划）
```

---

## 关键改造点

### 1. 固定数组定义

**原始代码**:
```cpp
vector<Complex> tcc;
tcc.resize(tccSize * tccSize);
```

**HLS改造**:
```cpp
// hls_ref_types.h
template<int SRC_SIZE, int LX, int LY, int NX_MAX, int NY_MAX>
struct FixedArrays {
    cmpxData_t tcc[(2*NX_MAX+1) * (2*NY_MAX+1) * (2*NX_MAX+1) * (2*NY_MAX+1)];
};
```

### 2. 固定循环边界

**原始代码**:
```cpp
// 动态边界
nxMinTmp = floor((-1 - sx) * Lx_norm);
nxMaxTmp = floor(( 1 - sx) * Lx_norm);
for (int nx = nxMinTmp; nx <= nxMaxTmp; nx++) { ... }
```

**HLS改造**:
```cpp
// 固定边界 + 条件判断
for (int ny_idx = 0; ny_idx < (2*NY_MAX+1); ny_idx++) {
    for (int nx_idx = 0; nx_idx < (2*NX_MAX+1); nx_idx++) {
#pragma HLS PIPELINE II=1
        int ny = ny_idx - NY_MAX;
        int nx = nx_idx - NX_MAX;
        // 内部判断是否在有效范围内
        data_t fx = nx / Lx_norm + sx;
        data_t fy = ny / Ly_norm + sy;
        if (fx*fx + fy*fy <= 1.0) { ... }
    }
}
```

### 3. HLS数学函数

**原始代码**:
```cpp
sqrt(1.0 - rho2 * NA * NA);
cos(phase);
sin(phase);
```

**HLS改造**:
```cpp
hls::sqrt(1.0 - rho2 * NA * NA);
hls::cos(phase);
hls::sin(phase);
```

---

## 使用说明

### 1. 作为HLS代码参考

```cpp
// 在实际HLS代码中引用
#include "hls_ref_types.h"

void calc_tcc_hls(
    hls::stream<cmpxData_t>& s_axis_src,
    // ... 其他接口
) {
    // 使用固定数组定义
    OpticalParams params; // 通过AXI-Lite传入
    
    // 使用固定循环边界模板
    calc_pupil_all_sources<SRC_SIZE, NX_MAX, NY_MAX>(...);
}
```

### 2. 配置参数选择

| 配置 | SRC_SIZE | LX/LY | NX_MAX/NY_MAX | 用途 |
|------|----------|-------|---------------|------|
| **最小** | 33 | 128 | 64 | CoSim验证 |
| **中等** | 51 | 256 | 128 | 功能验证 |
| **实际** | 101 | 2048 | 1024 | 生产部署 |

**注意**: 实际HLS代码中，参数通过AXI-Lite传入，数组尺寸通过模板参数固定。

---

## 下一步计划

| 文件 | 内容 | 状态 |
|------|------|------|
| `hls_ref_types.h` | 固定数组类型定义 | ✅ 完成 |
| `hls_ref_pupil.cpp` | Pupil Function计算 | ✅ 完成 |
| `hls_ref_tcc.cpp` | TCC矩阵累加 | ⬜ 待创建 |
| `hls_ref_fft.cpp` | hls::fft封装 | ⬜ 待创建 |
| `hls_ref_image.cpp` | calcImage实现 | ⬜ 待创建 |

---

## 参考资料

- **算法公式**: `reference/CPP_reference/Litho-TCC/ALGORITHM_MATH.md`
- **尺寸约束**: `reference/CPP_reference/Litho-TCC/ARRAY_SIZE_CONSTRAINTS.md`
- **原始代码**: `reference/CPP_reference/Litho-TCC/klitho_tcc.cpp`
- **HLS FFT参考**: `reference/vitis_hls_ftt的实现/interface_stream/fft_top.h`

---

**创建日期**: 2026-04-06
**版本**: v1.0
**状态**: 基础框架完成（pupil模块）