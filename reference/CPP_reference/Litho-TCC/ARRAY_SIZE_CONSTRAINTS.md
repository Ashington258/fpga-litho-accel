# HLS 数组尺寸约束文档

> **用途**：为 HLS 重构提供固定数组尺寸参考
> **创建日期**：2026-04-06
> **状态**：完整

---

## 1. 数组尺寸计算公式

### 1.1 基本尺寸参数

| 参数 | 计算公式 | 说明 |
|------|----------|------|
| \`Nx\` | \`floor(NA × Lx × (1 + outerSigma) / lambda)\` | TCC X 维度 |
| \`Ny\` | \`floor(NA × Ly × (1 + outerSigma) / lambda)\` | TCC Y 维度 |
| \`tccSize\` | \`(2×Nx+1) × (2×Ny+1)\` | TCC 矩阵总尺寸 |
| \`outerSigma\` | \`max_distance / center\` | 光源外缘相对半径 |

---

## 2. 尺寸配置方案

### 2.1 最小配置（CoSim 验证）

| 参数 | 值 | 说明 |
|------|-----|------|
| \`Lx, Ly\` | 128 | Mask 周期（小尺寸） |
| \`maskSizeX, maskSizeY\` | 64 | Mask 尺寸 |
| \`srcSize\` | 33 | 光源网格（小尺寸） |
| \`NA\` | 0.8 | 数值孔径（典型值） |
| \`lambda\` | 193 | 波长（ArF） |
| \`outerSigma\` | 0.9 | Annular光源外径 |
| \`Nx\` | ≈64 | \`floor(0.8×128×(1+0.9)/193)\` |
| \`Ny\` | ≈64 | 同上 |
| \`tccSize\` | ≈16641 | \`(2×64+1)² = 129×129\` |

**数组大小估算**：
- \`src[srcSize²]\` = 33×33 = 1089 float
- \`msk[Lx×Ly]\` = 128×128 = 16384 float
- \`tcc[tccSize²]\` = 16641×16641 ≈ 277M complex ❗**超大**

### 2.2 中等配置（功能验证）

| 参数 | 值 | 说明 |
|------|-----|------|
| \`Lx, Ly\` | 256 | Mask 周期 |
| \`srcSize\` | 51 | 光源网格 |
| \`Nx\` | ≈128 | \`floor(0.8×256×1.9/193)\` |
| \`tccSize\` | ≈66049 | \`(2×128+1)² = 257×257\` |

### 2.3 实际应用配置

| 参数 | 值 | 说明 |
|------|-----|------|
| \`Lx, Ly\` | 2048 | Mask 周期（实际尺寸） |
| \`srcSize\` | 101 | 光源网格（标准） |
| \`Nx\` | ≈1024 | \`floor(0.8×2048×1.9/193)\` |
| \`tccSize\` | ≈4M | \`(2×1024+1)² ≈ 4M\` ❗**极大** |

---

## 3. BRAM 容量约束

### 3.1 Kintex UltraScale+ (xcku3p-ffvb676-2-e)

| 资源 | 数量 | 单位容量 | 总容量 |
|------|------|----------|--------|
| BRAM | 216 | 36Kb | 7.7Mb ≈ 960KB |

**容量限制分析**：
- \`tcc[tccSize²×8字节]\` = 277M×8 ≈ 2.2GB（最小配置） ❗**远超BRAM**

### 3.2 存储策略

**✅ 可行方案**：

| 方案 | 存储位置 | 接口 | 说明 |
|------|----------|------|------|
| **方案1** | DDR | AXI-Master | TCC 全存DDR，分块读取 |
| **方案2** | Host端预处理 | AXI-Stream | Host 计算 pupil，FPGA 计算 TCC |

**推荐方案**：方案1（DDR 存储）

---

## 4. HLS 固定数组定义

### 4.1 最小配置示例

\`\`\`cpp
// hls_ref_types.h（最小配置）
const int SRC_SIZE = 33;
const int LX = 128;
const int LY = 128;
const int NX_MAX = 64;   // 最大Nx（用于固定数组）
const int NY_MAX = 64;   // 最大Ny
const int TCC_SIZE_MAX = (2*NX_MAX+1) * (2*NY_MAX+1);  // 16641

// 固定数组（用于 HLS）
typedef float data_t;
typedef hls::complex<data_t> cmpxData_t;

// 输入数组（可放入 BRAM）
data_t src[SRC_SIZE * SRC_SIZE];           // 1089 float ≈ 4KB
data_t msk[LX * LY];                        // 16384 float ≈ 64KB

// TCC 矩阵（必须使用 AXI-Master）
cmpxData_t *tcc;  // 指针，通过 AXI-Master 访问 DDR
\`\`\`

---

## 5. 配置约束汇总

| 配置 | Lx×Ly | srcSize | Nx×Ny | tccSize | 存储策略 |
|------|-------|---------|-------|---------|----------|
| **最小** | 128×128 | 33 | 64×64 | 16641 | DDR + 分块 |
| **中等** | 256×256 | 51 | 128×128 | 66049 | DDR + 分块 |
| **实际** | 2048×2048 | 101 | 1024×1024 | 4M | DDR + On-the-fly |

---

**创建日期**：2026-04-06
**参考代码**：\`reference/CPP_reference/Litho-TCC/klitho_tcc.cpp\`
**状态**：完整
