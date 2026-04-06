# SOCS HLS 算法流程分析与"简化版"说明

**分析日期**: 2026年4月6日  
**项目**: FPGA-Litho  
**模块**: SOCS HLS (calc_socs_simple_hls)

---

## 📊 当前HLS实现的算法流程

### 简化版本（`socs_simple_nofft.cpp`）

```mermaid
graph TB
    A[输入数据<br/>mskf: 512×512 complex<br/>krns: 10×9×9 complex<br/>scales: 10 float] --> B[Step 1: 循环nk=10个核]
    
    B --> C[核与Mask频域点乘<br/>提取中心区域 y∈[-4,4], x∈[-4,4]
    
    C --> D[❌ 关键简化点<br/>代替IFFT<br/>直接计算模平方 |z|²]
    
    D --> E[加权累加<br/>tmp_image += scales[k] × |z|²]
    
    E --> F[清零缓冲]
    
    F --> B
    
    B --> G[Step 2: fftshift<br/>17×17 → 17×17<br/>交换四象限]
    
    G --> H[⚠️ 简化插值<br/>最近邻插值<br/>17×17 → 512×512]
    
    H --> I[输出: 空中像<br/>512×512 float]
    
    style D fill:#ff6b6b,stroke:#c92a2a,stroke-width:3px,color:#fff
    style H fill:#ffd93d,stroke:#f59f00,stroke-width:2px
```

**核心步骤详解**：

| 步骤 | 操作 | 输入尺寸 | 输出尺寸 | 备注 |
|------|------|---------|---------|------|
| **Step 1.1** | 核与Mask频域点乘 | 9×9 | 17×17（填充） | 提取Mask中心9×9区域与核相乘 |
| **Step 1.2** | ❌ **直接计算模平方** | 17×17 complex | 17×17 float | **关键简化点：代替IFFT** |
| **Step 1.3** | 加权累加 | 17×17 × 10 | 17×17 | 对10个核的结果加权求和 |
| **Step 2** | fftshift | 17×17 | 17×17 | 交换四象限位置 |
| **Step 3** | ⚠️ **最近邻插值** | 17×17 | 512×512 | **简化插值：代替傅里叶插值** |

---

## 🔍 "简化版"的含义

### 简化的关键点

**简化版本跳过了两个关键步骤**：

#### 1. ❌ 跳过IFFT（逆傅里叶变换）

**完整版应该做**：
```cpp
// CPU参考实现（klitho_socs.cpp 第210-215行）
bwdDataIn[(difY + y + Ny) * sizeX + difX + x + Nx] = krns[k][...] * msk[...];
fftw_execute(bwd_plan);  // ← 真实的IFFT变换
tmpImg[i] += scales[k] * |bwdDataOut[i]|²;  // IFFT后再计算模平方
```

**简化版实际做了**：
```cpp
// HLS简化实现（socs_simple_nofft.cpp 第130-135行）
tmp_conv[conv_idx] = mskf[msk_idx] * krns[krn_idx];  // 频域点乘
// ❌ 没有IFFT！直接计算模平方
tmp_image[i] += scales[k] * (re² + im²);  // 频域数据直接算模平方
```

**影响**：
- 频域数据的模平方 ≠ 空域数据的模平方
- 缺少频域到空域的转换，物理意义不正确
- 导致输出数值远小于真实值（见验证结果对比）

#### 2. ⚠️ 跳过傅里叶插值

**完整版应该做**：
```cpp
// CPU参考实现（klitho_socs.cpp 第225行）
FI(tmpImgp, sizeX, sizeY, image, Lx, Ly, ...);  // 傅里叶插值
// 包含：R2C FFT → 频域zero-padding → C2R IFFT → fftshift
```

**简化版实际做了**：
```cpp
// HLS简化实现（socs_simple_nofft.cpp 第155行）
interp_simple(tmp_shifted, image, sizeX, sizeY, Lx, Ly);
// 最近邻插值：简单的像素复制，无频域处理
```

**影响**：
- 最近邻插值质量差，边缘锯齿明显
- 缺少频域扩展，图像细节丢失
- 无法正确放大图像

---

## 📈 完整版 vs 简化版对比

### 算法流程对比<tool_call>renderMermaidDiagram<arg_key>markup</arg_key><arg_value>graph LR
    subgraph Complete[完整版算法流程]
        A1[频域点乘] --> B1[IFFT变换<br/>频域→空域]
        B1 --> C1[空域模平方]
        C1 --> D1[加权累加]
        D1 --> E1[fftshift]
        E1 --> F1[傅里叶插值<br/>R2C→padding→C2R]
        F1 --> G1[输出空中像<br/>物理正确]
    end
    
    subgraph Simplified[简化版算法流程]
        A2[频域点乘] --> B2[❌ 直接模平方<br/>频域数据]
        B2 --> C2[加权累加]
        C2 --> D2[fftshift]
        D2 --> E2[⚠️ 最近邻插值]
        E2 --> F2[输出结果<br/>数值错误]
    end
    
    style B2 fill:#ff6b6b,stroke:#c92a2a
    style E2 fill:#ffd93d,stroke:#f59f00