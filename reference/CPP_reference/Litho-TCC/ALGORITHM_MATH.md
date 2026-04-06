# Litho-TCC 算法数学公式详解

> **用途**：为 HLS 重构提供明确的数学公式参考
> **创建日期**：2026-04-06
> **状态**：完整

---

## 1. Pupil Function 计算

### 1.1 基本参数定义

| 参数 | 含义 | 单位 |
|------|------|------|
| `lambda` | 波长 (ArF excimer) | nm |
| `NA` | 数值孔径 | 无量纲 |
| `defocus` | 离焦量（已归一化） | 无量纲 |
| `Lx, Ly` | Mask 周期 | nm |
| `Nx, Ny` | TCC 空间频率维度 | 无量纲 |
| `srcSize` | 光源网格尺寸 | 无量纲 |
| `outerSigma` | 光源外缘相对半径 | 无量纲 |

### 1.2 归一化参数

\`\`\`cpp
// 代码位置：klitho_tcc.cpp:calcTCC() 开头
float Lx_norm = Lx * NA / lambda;  // 空间频率归一化
float Ly_norm = Ly * NA / lambda;
float dz = defocus / (NA * NA / lambda);  // 实际离焦距离
float k = 2 * M_PI / lambda;  // 波数
\`\`\`

**HLS 实现要点**：
- 所有参数通过 AXI-Lite 传入，不硬编码
- 归一化计算在 calcTCC 内部完成
- \`lambda\` 固定为 193nm，但也可通过 AXI-Lite 配置

### 1.3 Pupil Function 公式

$$
P(s_x, s_y, n_x, n_y) = \exp\left(i \cdot k \cdot dz \cdot \sqrt{1 - \rho^2 \cdot NA^2}\right)
$$

其中：
- $s_x, s_y$：光源点坐标（归一化，范围 [-1, 1]）
- $n_x, n_y$：空间频率坐标（整数，范围与光源相关）
- $\rho^2 = f_x^2 + f_y^2$：归一化空间频率平方
- $f_x = \frac{n_x}{Lx_{norm}} + s_x$：总空间频率 X
- $f_y = \frac{n_y}{Ly_{norm}} + s_y$：总空间频率 Y

**代码对应**：
\`\`\`cpp
// klitho_tcc.cpp:calcTCC() 第260-270行
sx = (float)p / sh;  // s_x
sy = (float)q / sh;  // s_y
fx = (float)nx / Lx_norm + sx;  // f_x
fy = (float)ny / Ly_norm + sy;  // f_y
rho2 = fx * fx + fy * fy;       // ρ²
if (rho2 <= 1.0) {
  tmpValFloat = dz * k * (sqrt(1.0 - rho2 * NA * NA));
  pupil[...] = Complex(cos(tmpValFloat), sin(tmpValFloat));
}
\`\`\`

**HLS 实现要点**：
- 使用 \`hls::sqrt()\` 替换 \`sqrt()\`
- 使用 \`hls::cos()\` 和 \`hls::sin()\` 替换标准库函数
- 复数类型使用 \`hls::complex<float>\`
- 条件 \`rho2 <= 1.0\` 需保留（光瞳边界）

---

## 2. TCC 矩阵计算

### 2.1 TCC 定义

传输交叉系数（Transmission Cross Coefficient, TCC）定义为：

$$
TCC(n_1, n_2) = \sum_{p,q} S(p,q) \cdot P(p,q,n_1) \cdot P^*(p,q,n_2)
$$

其中：
- $S(p,q)$：光源强度分布（归一化，总和为 1）
- $P(p,q,n)$：Pupil Function（见上节）
- $P^*$：复共轭
- $(p,q)$：光源网格坐标
- $(n_1, n_2)$：空间频率坐标对

### 2.2 循环边界计算

**光源循环边界**：
\`\`\`cpp
int sh = (srcSize - 1) / 2;  // 光源中心偏移
int oSgm = ceil(sh * outerSigma);  // 光源有效半径
// 循环范围：for (q=-oSgm; q<=oSgm; q++) { for (p=-oSgm; p<=oSgm; p++) }
\`\`\`

**空间频率循环边界**（动态）：
\`\`\`cpp
// 对于每个光源点 (sx, sy)：
nxMinTmp = floor((-1 - sx) * Lx_norm);  // 最小 nx
nxMaxTmp = floor(( 1 - sx) * Lx_norm);  // 最大 nx
nyMinTmp = floor((-1 - sy) * Ly_norm);  // 最小 ny
nyMaxTmp = floor(( 1 - sy) * Ly_norm);  // 最大 ny
\`\`\`

**边界约束**：
- 循环范围受光瞳边界约束：$\rho^2 = f_x^2 + f_y^2 \leq 1$
- 实际有效范围：$|n_x| \leq Nx$, $|n_y| \leq Ny$

### 2.3 Pupil 矩阵索引映射

**⚠️ 重要**：pupil矩阵是2D→1D映射，理解索引是HLS实现的关键。

\`\`\`cpp
// Pupil 矩阵尺寸：
int pupilRows = (2 * Nx + 1) * (2 * Ny + 1);  // 空间频率维度（行）
int pupilCols = srcSize * srcSize;            // 光源点数量（列）

// 空间频率索引（ID1）：
// ny ∈ [-Ny, +Ny], nx ∈ [-Nx, +Nx]
ID1 = (ny + Ny) * (2 * Nx + 1) + (nx + Nx);  // 行索引，范围 [0, tccSize-1]

// 光源点索引（srcID）：
// q ∈ [-sh, +sh], p ∈ [-sh, +sh]
srcID = (q + sh) * srcSize + (p + sh);       // 列索引，范围 [0, srcSize²-1]

// Pupil 存储：
pupil[ID1][srcID] = Complex(cos(phase), sin(phase));
\`\`\`

**索引示意图**：
```
pupil 矩阵结构（示例：Nx=2, Ny=2, srcSize=5）:

空间频率维度 (5×5 = 25行)
┌─────────────────────────────────────────┐
│ ID1=0  (ny=-2, nx=-2): [srcID=0..24]    │ ← pupil[0][srcID]
│ ID1=1  (ny=-2, nx=-1): [srcID=0..24]    │
│ ...                                     │
│ ID1=12 (ny=0,  nx=0 ): [srcID=0..24]    │ ← 中心频率
│ ...                                     │
│ ID1=24 (ny=2,  nx=2 ): [srcID=0..24]    │
└─────────────────────────────────────────┘
         光源点维度 (25列)
```

### 2.4 TCC 存储索引

\`\`\`cpp
// TCC 矩阵尺寸：
int tccSize = (2 * Nx + 1) * (2 * Ny + 1);

// 索引计算：
ID1 = (ny + Ny) * (2 * Nx + 1) + (nx + Nx);  // n1 的线性索引
ID2 = (my + Ny) * (2 * Nx + 1) + (mx + Nx);  // n2 的线性索引

// TCC 存储：
tcc[ID1 * tccSize + ID2] = ...;  // 2D → 1D 映射
\`\`\`

**⚠️ HLS注意事项**：
- pupil矩阵存储策略：每个光源点的pupil向量可存BRAM，但全部pupil矩阵需DDR
- 推荐策略：**逐光源点计算**，避免存储完整pupil矩阵（On-the-fly）

### 2.5 共轭对称性填充

TCC 具有共轭对称性：

$$
TCC(n_2, n_1) = TCC^*(n_1, n_2)
$$

**代码实现**：
\`\`\`cpp
// klitho_tcc.cpp:calcTCC() 第330-335行
for (int i = 0; i < tccSize; i++) {
  for (int j = i + 1; j < tccSize; j++) {
    tcc[j * tccSize + i] = conj(tcc[i * tccSize + j]);
  }
}
\`\`\`

**HLS 实现要点**：
- 只计算上三角，下三角通过共轭填充
- 减少计算量约 50%

---

## 3. FFT 相关计算

### 3.1 fftshift 操作

**目的**：将零频移到中心，便于后续处理

$$
\text{shift}(x) = (x + \text{offset}) \mod \text{size}
$$

**代码实现**：
\`\`\`cpp
// klitho_tcc.cpp:myShift() 第24-36行
int xh = shiftTypeX ? sizeX / 2 : (sizeX + 1) / 2;
int yh = shiftTypeY ? sizeY / 2 : (sizeY + 1) / 2;
for (int y = 0; y < sizeY; y++) {
  for (int x = 0; x < sizeX; x++) {
    sy = (y + yh) % sizeY;
    sx = (x + xh) % sizeX;
    out[sy * sizeX + sx] = in[y * sizeX + x];
  }
}
\`\`\`

**HLS 实现策略**：
- **方案1**：Host 端预处理，HLS 接收已 shift 的数据
- **方案2**：使用 Line Buffer + 双端口 BRAM 实现
- **方案3**：利用 hls::fft 内置 shift 功能

**推荐方案**：方案1（Host 端预处理），减少 HLS 复杂度

### 3.2 FFT 复数重组（FT_r2c 核心）

**⚠️ 重要**：FFTW3输出是压缩格式（只输出正频率），需重组为完整复数矩阵。

#### 3.2.1 FFTW3输出格式

FFTW3的r2c输出格式（对于sizeX×sizeY输入）：
- 输出尺寸：`(sizeX/2+1) × sizeY`（只存储正频率）
- 实际需要尺寸：`sizeX × sizeY`（完整复数矩阵）

#### 3.2.2 复数重组公式

**Right Half (R)**：正频率部分
\`\`\`cpp
// klitho_tcc.cpp:FT_r2c() 第110-120行
int wt = (sizeX / 2) + 1;      // FFTW输出宽度
int AY = (sizeY + 1) / 2;      // 上半部分
int BY = sizeY - AY;           // 下半部分

// 提取上半部分（AY~sizeY）
for (int y = 0, y2 = AY; y < BY; y++, y2++) {
  for (int x = 0; x < wt; x++) {
    R[y * wt + x] = datac[y2 * wt + x];  // FFTW输出的上半部分
  }
}

// 提取下半部分（0~AY）
for (int y = BY, y2 = 0; y < sizeY; y++, y2++) {
  for (int x = 0; x < wt; x++) {
    R[y * wt + x] = datac[y2 * wt + x];  // FFTW输出的下半部分
  }
}
\`\`\`

**Left Half (L)**：负频率部分（通过共轭对称性计算）
\`\`\`cpp
// 偶数尺寸：对称共轭
if (sizeY % 2 == 0) {
  for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
    L[x] = conj(R[x2]);  // 首行对称
  }
  for (int y = 1, y2 = sizeY - 1; y < sizeY; y++, y2--) {
    for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
      L[y * wt + x] = conj(R[y2 * wt + x2]);  // 其余行对称
    }
  }
}

// 奇数尺寸：直接共轭
else {
  for (int y = 0, y2 = sizeY - 1; y < sizeY; y++, y2--) {
    for (int x = 0, x2 = wt - 1; x < wt; x++, x2--) {
      L[y * wt + x] = conj(R[y2 * wt + x2]);
    }
  }
}
\`\`\`

**最终输出**：
\`\`\`cpp
// 组合L和R为完整复数矩阵
for (int y = 0; y < sizeY; y++) {
  for (int x = 0; x < sizeXh; x++) {       // 左半部分（负频率）
    out[y * sizeX + x] = L[y * wt + x];
  }
  for (int x = sizeXh, x2 = 0; x < sizeX; x++, x2++) {  // 右半部分（正频率）
    out[y * sizeX + x] = R[y * wt + x2];
  }
}

// 归一化
for (int i = 0; i < sizeX * sizeY; i++) {
  out[i] /= (sizeX * sizeY);
}
\`\`\`

#### 3.2.3 共轭对称性数学公式

$$
F(-f_x, -f_y) = F^*(f_x, f_y)
$$

其中：
- $F(f_x, f_y)$：频率域复数值
- $F^*$：复共轭
- $-f_x$：负频率（Left Half）
- $f_x$：正频率（Right Half）

**HLS 实现要点**：
- FFTW3的压缩格式不适用于HLS，需使用hls::fft的完整输出模式
- hls::fft输出可选完整复数矩阵，无需手动重组
- **推荐**：使用hls::fft的`pipelined_streaming_io`架构，自动处理输出格式

---

## 4. calcImage 频域卷积

### 4.1 calcImage 定义

频域卷积计算空中像：

$$
I(n_2) = \sum_{n_1} M(n_1) \cdot M^*(n_2 + n_1) \cdot TCC(n_1, n_2 + n_1)
$$

其中：
- $M(n)$：Mask的频域表示（FFT输出）
- $M^*$：复共轭
- $n_1$：卷积核偏移
- $n_2$：输出空间频率坐标

### 4.2 循环边界（⚠️ 非对称）

\`\`\`cpp
// klitho_tcc.cpp:calcImage() 第340-360行
int Lxh = Lx / 2;  // Mask中心偏移
int Lyh = Ly / 2;

// 外层循环：输出坐标（非对称范围）
for (int ny2 = -2 * Ny; ny2 <= 2 * Ny; ny2++) {      // Y: [-2Ny, +2Ny]
  for (int nx2 = 0 * Nx; nx2 <= 2 * Nx; nx2++) {     // X: [0, +2Nx] ⚠️ 注意起点
    val = 0;
    
    // 内层循环：卷积核偏移
    for (int ny1 = -1 * Ny; ny1 <= Ny; ny1++) {      // Y: [-Ny, +Ny]
      for (int nx1 = -1 * Nx; nx1 <= Nx; nx1++) {    // X: [-Nx, +Nx]
        
        // 边界检查
        if (abs(nx2 + nx1) <= Nx && abs(ny2 + ny1) <= Ny) {
          // Mask索引
          idx1 = (ny1 + Lyh) * Lx + (nx1 + Lxh);
          idx2 = (ny2 + ny1 + Lyh) * Lx + (nx2 + nx1 + Lxh);
          
          // TCC索引
          tcc_idx1 = (ny1 * (2 * Nx + 1) + nx1 + tccSizeh);
          tcc_idx2 = ((ny2 + ny1) * (2 * Nx + 1) + (nx2 + nx1) + tccSizeh);
          
          val += msk[idx2] * conj(msk[idx1]) * tcc[tcc_idx1 * tccSize + tcc_idx2];
        }
      }
    }
    imgf[(ny2 + Lyh) * Lx + (nx2 + Lxh)] = val;
  }
}
\`\`\`

### 4.3 索引映射公式

**Mask频域索引**：
$$
\text{idx} = (n_y + L_y/2) \cdot L_x + (n_x + L_x/2)
$$

**TCC索引**：
$$
\text{tcc\_idx} = [(n_y + N_y) \cdot (2N_x+1) + (n_x + N_x)]
$$

**输出索引**：
$$
\text{img\_idx} = (n_{y2} + L_y/2) \cdot L_x + (n_{x2} + L_x/2)
$$

### 4.4 HLS 实现要点

| 要点 | 说明 |
|------|------|
| **循环边界固定化** | 外层：`ny2∈[-2Ny, 2Ny]`，内层：`ny1∈[-Ny, Ny]`（固定范围） |
| **边界条件判断** | `if (abs(nx2+nx1) <= Nx && abs(ny2+ny1) <= Ny)` 需保留 |
| **TCC分块加载** | TCC矩阵大，需分块从DDR加载（使用AXI-Master） |
| **Mask存储** | Mask频域数据可存BRAM（尺寸：Lx×Ly ≈ 64KB） |

---

## 5. HLS 实现关键公式汇总

| 模块 | 核心公式 | HLS 需替换函数 |
|------|----------|----------------|
| Pupil | $P = \exp(i \cdot k \cdot dz \cdot \sqrt{1 - \rho^2 NA^2})$ | sqrt→hls::sqrt, cos/sin→hls::cos/sin |
| TCC | $TCC = \sum S \cdot P_1 \cdot P_2^*$ | 无需替换 |
| FFT shift | $shift(x) = (x + offset) \mod size$ | Host预处理或Line Buffer |

---

## 6. 参考代码位置索引

| 函数 | 代码位置 | 行数 | 关键内容 |
|------|----------|------|----------|
| \`myShift()\` | klitho_tcc.cpp:24-36 | ~12行 | fftshift操作 |
| \`getOuterSigma()\` | klitho_tcc.cpp:45-60 | ~15行 | Annular光源外径计算 |
| \`FT_r2c()\` | klitho_tcc.cpp:100-150 | ~50行 | **FFT复数重组（L/R分离）** |
| \`FT_c2r()\` | klitho_tcc.cpp:160-190 | ~30行 | FFT逆变换+复数重组 |
| \`calcTCC()\` | klitho_tcc.cpp:200-335 | ~135行 | Pupil计算+TCC累加+共轭填充 |
| \`calcImage()\` | klitho_tcc.cpp:340-360 | ~20行 | **频域卷积（非对称循环边界）** |
| \`calcKernels()\` | klitho_tcc.cpp:360-450 | ~90行 | 特征值分解（Host端执行） |

---

**创建日期**：2026-04-06
**更新日期**：2026-04-06（补充FFT复数重组、calcImage索引公式）
**参考代码**：\`reference/CPP_reference/Litho-TCC/klitho_tcc.cpp\`
**状态**：完整（Phase 0-4 所有核心算法已覆盖）
