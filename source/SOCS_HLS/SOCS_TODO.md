# SOCS HLS 重构 TODO

## 项目目标

将 `reference/CPP_reference/Litho-SOCS/klitho_socs.cpp` 中的 **calcSOCS** 算法进行工程改写，生成 Vitis HLS IP 核。

**关键决策**：
- 第一版 HLS IP 采用 **算法改写版**，不是对原始 `calcSOCS` 的严格等价重构
- 改写点：将原始 65×65 IFFT 替换为 **128×128 zero-padded 2D C2C IFFT**
- FI（Fourier Interpolation）步骤留在 Host 端，HLS 仅输出 `tmpImgp[65×65]`
- 第一版系统接口采用 **AXI-MM（m_axi）**，面向 DDR/全局存储器访问

**系统功能**：
- 输入数据存放于外部存储器
- IP 核通过 AXI-MM 读取 mask 频域数据、SOCS 核和特征值
- 输出中间结果 `tmpImgp[65×65]`
- Host 端负责 FI 插值得到最终空中像

---

## 核心原则（必须明确）

本文档遵循以下 **5 条核心原则**：

1. **第一版 HLS IP 采用算法改写版，需将 litho.cpp 中的 17×17 IFFT 改写为 32×32 zero-padded IFFT（基于当前配置 Nx=4）。**
2. **改写原因：Vitis HLS FFT 只支持 2 的幂次长度，litho.cpp 已通过 nextPowerOfTwo() 自动填充到 2^N。**
3. **Nx, Ny 是动态计算参数，公式：$N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$**
4. **HLS 验证 golden 需新编写改写版 CPU reference，输出 tmpImgp_pad32.bin（17×17，基于当前配置）。**
5. **litho.cpp 当前输出是 512×512 aerial_image，不是 17×17 tmpImgp，需提取中间步骤。**

**⚠️ 重要说明 - 配置路径选择**：

**方案 A：当前配置路径（推荐用于快速验证）**
- **当前 litho.cpp 配置**：`Nx = Ny = 4`（基于 `NA=0.8, Lx=512, λ=193, σ=0.9`）
- **当前 IFFT 尺寸**：`17×17 → 32×32 zero-padded IFFT`（已满足 2^N 标准）
- **HLS 目标**：实现 `32×32 2D IFFT`，输出 `tmpImgp[17×17]`
- **优点**：与现有 litho.cpp 一致，验证流程简单

**方案 B：目标配置路径（用于未来高精度系统）**
- **目标配置**：`Nx = Ny = 16`，需修改 config.json（NA=1.2 或 Lx=1536）
- **目标 IFFT 尺寸**：`65×65 → 128×128 zero-padded IFFT`
- **HLS 目标**：实现 `128×128 2D IFFT`，输出 `tmpImgp[65×65]`
- **需执行**：修改配置参数并重新运行 litho.cpp 生成 golden 数据

---

## 术语说明

- **原始算法版**：指 `klitho_socs.cpp` 中使用 65×65 复数 IFFT 的版本
- **改写版算法**：指使用 **128×128 zero-padded C2C IFFT** 的 HLS 目标版本
- **golden**：若无特殊说明，均指改写版 CPU reference 生成的数据

---

## 算法分析（改写版）

### calcSOCS（HLS 改写版）核心流程

**输入**：
- `mskf[Lx×Ly]`：已在 Host 端完成 FFT 的 mask 频域数据（complex<float>）
- `krns[nk × (2Nx+1) × (2Ny+1)]`：SOCS 核数组（complex<float>）
- `scales[nk]`：特征值权重（float）

**参数**：
- `Lx, Ly, Nx, Ny, nk`
- **Nx, Ny 计算公式**：$N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$（动态计算）

**方案 A：当前配置（推荐验证路径）**
- **实际值**：`Nx = Ny = 4`（基于 `NA=0.8, Lx=512, λ=193, σ=0.9`）
- **物理卷积尺寸**：`convX = 4×Nx+1 = 17×17`
- **IFFT 执行尺寸**：`fftConvX = 32×32`（通过 `nextPowerOfTwo(17)` 自动填充）
- **✅ litho.cpp 已满足 2^N 标准**

**方案 B：目标配置（未来高精度路径）**
- **目标值**：`Nx = Ny = 16`（需修改 config.json）
- **物理卷积尺寸**：`convX = 4×Nx+1 = 65×65`
- **IFFT 执行尺寸**：`fftConvX = 128×128`（通过 `nextPowerOfTwo(65)` 自动填充）
- **配置修改**：`NA=1.2` 或 `Lx=1536`

**流程**：

```
对每个 kernel k = 0 .. nk-1:
    1. 从 mskf 中提取中心 (2Nx+1) × (2Ny+1) 频域区域
       【方案A】(9×9) 【方案B】(33×33)
    2. 与 krns[k] 做逐点复数乘法
    3. 将乘积结果按照改写版布局写入复数 IFFT 输入矩阵，其余位置补零
       【方案A】32×32 矩阵 【方案B】128×128 矩阵
       【关键】build_padded_ifft_input(...) 定义布局规则
    4. 执行 2D C2C IFFT（行列分解 + 转置）
       【方案A】32×32 【方案B】128×128
    5. 从 IFFT 结果中提取有效输出区域
       【方案A】17×17 【方案B】65×65
       【关键】extract_valid_output(...) 提取规则必须与 CPU 改写版一致
    6. 执行强度累加：tmpImg += scales[k] * (re^2 + im^2)

对累加结果执行 fftshift：
    输出 tmpImgp
    【方案A】17×17 【方案B】65×65

Host 端后续处理：
    对 tmpImgp 执行 FI，得到最终 image[512×512]
```

**重要说明**：
- 步骤 3 的布局规则由改写版算法定义，必须先由 CPU reference 固化
- 步骤 5 的提取区域必须与改写版 CPU reference 完全一致
- HLS 不保证与原始 65×65 IFFT 严格等价

---

## 数据规模与存储约束

### 关键数据规模

| 数据项 | 方案A尺寸 | 方案B尺寸 | 类型 | 方案A大小 | 方案B大小 | 存储位置 |
|--------|-----------|-----------|------|-----------|-----------|----------|
| `mskf` | 512×512 | 512×512 | complex<float> | **2 MB** | **2 MB** | DDR（AXI-MM）|
| `krns` | 16×9×9 | 16×33×33 | complex<float> | ~1.3 KB | ~70 KB | 本地缓存 |
| `scales` | 16 | 16 | float | 64 B | 64 B | 本地缓存 |
| `tmpImgp` | 17×17 | 65×65 | float | **1.1 KB** | **16.9 KB** | BRAM |
| 临时IFFT缓冲 | 32×32 | 128×128 | complex<float> | ~8 KB | ~128 KB | BRAM（分区）|

**存储决策**：
- `mskf` 规模固定（512×512），必须通过 AXI-MM 访问 DDR
- `krns` 方案A极小（~1.3 KB），方案B适中（~70 KB），均可本地缓存
- `tmpImgp` 方案A更小（1.1 KB），方案B适中（16.9 KB），均可放入 BRAM

**存储决策**：
- `mskf` 规模过大，必须通过 AXI-MM 按需 burst 读取中心区域
- `krns` 和 `scales` 可考虑本地缓存
- `tmpImgp[65×65]` 为 `float` 实数数组，大小约 **16.9 KB**，可放入 BRAM

---

## 阶段 -1：改写版算法基线确认（前置阶段）

**目标**：建立改写版算法的 CPU reference，作为后续 HLS 验证的唯一 golden 标准。

### 任务列表

- [ ] **提取原始算法输出**
  - 基于原始 `calcSOCS` 提取一个 CPU 参考实现
  - 输出 `tmpImgp_orig_65.bin`（65×65，float，row-major）
  - 用于评估算法改写引入的偏差

- [ ] **编写改写版 CPU reference**
  - **方案A（推荐）**：从 litho.cpp 提取当前 `32×32 zero-padded IFFT` 实现
  - **方案B（目标）**：修改配置后实现 `128×128 zero-padded IFFT`
  - 实现 `build_padded_ifft_input(...)`：定义补零布局
  - 实现 `extract_valid_output(...)`：定义提取区域
  - 输出 golden 文件：
    - **方案A**：`tmpImgp_pad32.bin`（17×17，float，row-major）
    - **方案B**：`tmpImgp_pad128.bin`（65×65，float，row-major）
  - **此文件为后续 HLS 验证的唯一 golden**

- [ ] **算法改写偏差评估**
  - 对比 `tmpImgp_orig_65.bin` 与 `tmpImgp_pad128.bin`
  - 记录：
    - RMSE（归一化均方根误差）
    - 最大绝对误差
    - 峰值位置偏差
  - **明确**：此偏差仅用于评估算法改写影响，不作为 HLS pass/fail 标准

- [ ] **文档化改写版算法定义**
  - 详细记录 `build_padded_ifft_input` 的补零布局规则
  - 详细记录 `extract_valid_65x65` 的提取区域定义
  - 确保后续 HLS 实现严格遵循此定义

**验收标准**：
- 生成 `tmpImgp_pad128.bin` 作为 HLS 唯一 golden
- 算法改写偏差有明确量化评估
- 布局规则文档化

---

## 阶段 0：基础设施 + Golden 数据

### 任务列表

- [ ] **创建目录结构**
  - `source/SOCS_HLS/src/` - HLS源代码
  - `source/SOCS_HLS/tb/` - Testbench
  - `source/SOCS_HLS/data/` - Golden测试数据
  - `source/SOCS_HLS/script/` - TCL脚本
  - `source/SOCS_HLS/hls/` - HLS工作目录

- [ ] **配置环境**
  - 目标器件: `xcku3p-ffvb676-2-e`
  - 时钟频率: 200 MHz (5ns)
  - Vitis HLS 2025.2

- [ ] **准备 Golden 数据**
  - 从 `verification/` 或改写版 CPU reference 提取：
    - `mskf_r.bin`, `mskf_i.bin`（mask频域，512×512）
    - `scales.bin`（特征值，nk=16）
    - `krns_r/i.bin`（SOCS核）
      - **方案A**：`16×9×9`（基于 Nx=4）
      - **方案B**：`16×33×33`（基于 Nx=16）
    - **HLS golden 文件**（取决于选择的配置路径）：
      - **方案A**：`tmpImgp_pad32.bin`（17×17，float）
      - **方案B**：`tmpImgp_pad128.bin`（65×65，float）
    - 可选保留：`tmpImgp_orig.bin`（仅用于算法偏差分析）
    - `image.bin`（仅用于 Host+HLS 联合流程验证）

- [ ] **明确数据格式规范**
  - `float32`（4 字节/元素）
  - `row-major` 存储
  - `complex` 存储顺序：`[real, imag]` 交替
  - `tmpImgp`：已 shift 的实数数组

- [ ] **编写基础 TCL 脚本**
  - `script/run_csynth.tcl`
  - `script/run_cosim.tcl`
  - `script/run_package.tcl`

---

## 阶段 1：官方 Vitis HLS 1D C2C FFT 可用性验证

**目标**：验证官方 Vitis HLS FFT 核在 2^N 点 FFT/IFFT 上的可用性与精度。

**方案A（当前配置）**：验证 32 点 FFT/IFFT
**方案B（目标配置）**：验证 128 点 FFT/IFFT

### 任务列表

- [ ] **数据类型定义**
  ```cpp
  typedef float data_t;
  typedef std::complex<data_t> cmpxData_t;  // 与官方 hls::fft 兼容
  ```

- [ ] **参考官方 FFT 示例**
  - 参考：`reference/vitis_hls_fft的实现/interface_stream/`
  - 认 `hls::fft` 的使用方式、参数配置

- [ ] **1D C2C FFT 验证函数**
  ```cpp
  void fft_1d_c2c(
      hls::stream<cmpxData_t>& in,
      hls::stream<cmpxData_t>& out,
      int nfft,        // log2(N)
                     // 方案A: 固定为 5 (32点)
                     // 方案B: 固定为 7 (128点)
      bool inverse     // true=IFFT
  );
  ```

- [ ] **验证内容**
  - 数据类型是否采用 `std::complex<float>` 或兼容类型
  - `inverse` 参数的 scaling 规则（IFFT 是否需要手动缩放）
  - 若只做 128 点，则建议固定 `nfft=7`，简化运行时配置
  - 若需支持可变尺寸，验证 runtime `nfft` 是否可用

- [ ] **精度测试**
  - 输入：随机复数序列（128点）
  - 对比：HLS FFT vs CPU FFTW/参考实现
  - 记录：最大误差、RMSE
  - 目标量级：`1e-5 ~ 1e-4`

**验收标准**：
- 128 点 1D FFT/IFFT 可正常调用
- 误差在 `1e-5 ~ 1e-4` 量级
- scaling 规则明确

**不再包含**：
- 与 65 点 FFT 相关的任何内容

---

## 阶段 2：128×128 2D C2C IFFT（行列分解 + 转置）

**目标**：实现 2^N × 2^N 固定尺寸 2D IFFT 并验证精度。

**方案A（当前配置）**：实现 32×32 2D IFFT
**方案B（目标配置）**：实现 128×128 2D IFFT

### 任务列表

- [ ] **2D IFFT 主函数**
  ```cpp
  // 方案A（当前配置）
  void ifft_2d_32x32(
      cmpxData_t in[32][32],   // 输入：已零填充
      cmpxData_t out[32][32],  // 输出：IFFT 结果
      bool inverse = true      // IFFT
  );
  
  // 方案B（目标配置）
  void ifft_2d_128x128(
      cmpxData_t in[128][128], // 输入：已零填充
      cmpxData_t out[128][128],// 输出：IFFT 结果
      bool inverse = true      // IFFT
  );
  ```

- [ ] **Row-FFT 实现**
  ```cpp
  // 方案A（当前配置）
  void ifft_2d_rows_32(
      cmpxData_t matrix[32][32],
      int nfft = 5,              // 固定 32 点
      bool inverse = true
  );
  
  // 方案B（目标配置）
  void ifft_2d_rows_128(
      cmpxData_t matrix[128][128],
      int nfft = 7,              // 固定 128 点
      bool inverse = true
  );
  ```

- [ ] **矩阵转置模块**
  ```cpp
  // 方案A（当前配置）
  void transpose_matrix_32(
      cmpxData_t in[32][32],
      cmpxData_t out[32][32]
  );
  
  // 方案B（目标配置）
  void transpose_matrix_128(
      cmpxData_t in[128][128],
      cmpxData_t out[128][128]
  );
  ```
  - 第一版：朴素 BRAM 数组实现
  - 后续优化：Line Buffer 优化

- [ ] **零填充辅助函数**
  ```cpp
  // 方案A（当前配置）
  void build_padded_ifft_input_32(
      cmpxData_t product[9][9],    // 频域点乘结果 (2*Nx+1)
      cmpxData_t padded[32][32],   // 零填充后
      // 布局规则由改写版算法定义
  );
  
  // 方案B（目标配置）
  void build_padded_ifft_input_128(
      cmpxData_t product[33][33],  // 频域点乘结果 (2*Nx+1)
      cmpxData_t padded[128][128], // 雟填充后
      // 布局规则由改写版算法定义
  );
  ```
  - **关键**：补零布局必须与改写版 CPU reference 一致

- [ ] **有效区域提取**
  ```cpp
  // 方案A（当前配置）
  void extract_valid_17x17(
      cmpxData_t ifft_out[32][32],   // IFFT 全输出
      cmpxData_t valid[17][17],      // 提取有效区域 (4*Nx+1)
      // 提取规则由改写版算法定义
  );
  
  // 方案B（目标配置）
  void extract_valid_65x65(
      cmpxData_t ifft_out[128][128], // IFFT 全输出
      cmpxData_t valid[65][65],      // 提取有效区域 (4*Nx+1)
      // 提取规则由改写版算法定义
  );
  ```
  - **关键**：提取区域必须与改写版 CPU reference 完全一致

- [ ] **2D IFFT 精度验证**
  - 输入：已零填充的 128×128 复数矩阵
  - 流程：行 IFFT → 转置 → 列 IFFT
  - 对比：改写版 CPU reference 的 128×128 IFFT 结果
  - 误差目标：`1e-4` 量级

**验收标准**：
- 128×128 2D IFFT 可正常运行
- 提取的 65×65 区域与改写版 CPU reference 一致
- 误差在 `1e-4` 量级

---

## 阶段 3：改写版 calcSOCS 核心实现

**目标**：实现频域点乘 + IFFT + 累加的完整流程。

### 核心函数拆分（方案A：当前配置 Nx=4）

```cpp
// 1. 构建零填充的 IFFT 输入
void build_padded_ifft_input_32(
    cmpxData_t* mskf,           // mask 频域中心区域 (9×9)
    cmpxData_t* krn,            // 单个 SOCS 核 (9×9)
    cmpxData_t padded[32][32], // 输出：零填充
    int Nx, int Ny, int Lxh, int Lyh
);

// 2. 32×32 2D IFFT
void ifft2d_32x32(
    cmpxData_t in[32][32],
    cmpxData_t out[32][32]
);

// 3. 提取有效 17×17 区域
void extract_valid_17x17(
    cmpxData_t in[32][32],
    cmpxData_t out[17][17]
);

// 4. 强度累加
void accumulate_intensity_17x17(
    cmpxData_t ifft_out[17][17],  // IFFT 输出的有效区域
    data_t* accum_image,          // 累加图像
    data_t scale,                 // 特征值权重
    int sizeX, int sizeY          // 固定为 17×17
);
// 公式：accum += scale × (re² + im²)

// 5. fftshift
void fftshift_17x17(
    data_t* in,
    data_t* out,
    int sizeX, int sizeY
);
```

### 核心函数拆分（方案B：目标配置 Nx=16）

```cpp
// 1. 构建零填充的 IFFT 输入
void build_padded_ifft_input_128(
    cmpxData_t* mskf,           // mask 频域中心区域 (33×33)
    cmpxData_t* krn,            // 单个 SOCS 核 (33×33)
    cmpxData_t padded[128][128],// 输出：零填充
    int Nx, int Ny, int Lxh, int Lyh
);

// 2. 128×128 2D IFFT
void ifft2d_128x128(
    cmpxData_t in[128][128],
    cmpxData_t out[128][128]
);

// 3. 提取有效 65×65 区域
void extract_valid_65x65(
    cmpxData_t in[128][128],
    cmpxData_t out[65][65]
);

// 4. 强度累加
void accumulate_intensity_65x65(
    cmpxData_t ifft_out[65×65],  // IFFT 输出的有效区域
    data_t* accum_image,          // 累加图像
    data_t scale,                 // 特征值权重
    int sizeX, int sizeY          // 固定为 65×65
);
// 公式：accum += scale × (re² + im²)

// 5. fftshift
void fftshift_65x65(
    data_t* in,
    data_t* out,
    int sizeX, int sizeY
);
```

### 主函数流程

```cpp
void calc_socs_core(
    // AXI-MM 输入
    cmpxData_t* mskf,        // mask频域 (512×512)
    cmpxData_t* krns,        // SOCS核数组
                             // 方案A: nk × 9×9
                             // 方案B: nk × 33×33
    data_t* scales,         // 特征值 (nk)
    
    // AXI-MM 输出
    data_t* tmpImgp,        // 中间输出
                             // 方案A: 17×17
                             // 方案B: 65×65
    
    // AXI-Lite 参数
    int Lx, int Ly, int Nx, int Ny, int nk
) {
    // 初始化累加器
    // 方案A: data_t tmpImg[17*17] = {0};
    // 方案B: data_t tmpImg[65*65] = {0};
    
    for (int k = 0; k < nk; k++) {
        // 1. 构建零填充的 IFFT 输入
        // 方案A: cmpxData_t padded[32*32];
        // 方案B: cmpxData_t padded[128*128];
        build_padded_ifft_input(mskf, &krns[k*kerSize], padded, Nx, Ny, Lx/2, Ly/2);
        
        // 2. 执行 2D IFFT
        // 方案A: ifft2d_32x32(padded, ifft_out);
        // 方案B: ifft2d_128x128(padded, ifft_out);
        
        // 3. 提取有效区域
        // 方案A: extract_valid_17x17(ifft_out, valid);
        // 方案B: extract_valid_65x65(ifft_out, valid);
        
        // 4. 强度累加
        accumulate_intensity(valid, tmpImg, scales[k], outSizeX, outSizeY);
    }
    
    // 5. fftshift
    fftshift(tmpImg, tmpImgp, outSizeX, outSizeY);
}
```

### 关键说明

1. **`build_padded_ifft_input(...)` 是改写版算法定义的关键函数**
   - 定义 `mskf` 中心区域索引方式
   - 定义 `krn` 的映射方式
   - 定义在 128×128 IFFT 输入矩阵中的写入位置
   - **该布局必须先由 CPU 改写版 reference 固化，再由 HLS 严格复现**

2. **`extract_valid_65x65(...)` 提取规则**
   - 必须与改写版 CPU reference 完全一致
   - 提取位置和范围必须明确定义

### 验证

- HLS 输出 `tmpImgp`
- 与 `tmpImgp_pad128.bin` 对比
- 不与原始 `tmpImgp_orig_65.bin` 直接做 pass/fail
- 误差目标：归一化后 RMSE < 1e-4 或根据实际情况评估

---

## 阶段 4：Top-level 接口与 Testbench

**目标**：完成完整 IP 接口并验证。

### Top 函数（AXI 接口规范）

**方案A（当前配置 Nx=4）**：
```cpp
void socs_top(
    // AXI-MM 输入/输出
    cmpxData_t* mskf,        // depth=512*512 (2 MB, DDR访问)
    cmpxData_t* krns,        // depth=16*9*9 (~1.3 KB, 可缓存)
    data_t* scales,          // depth=16 (64 B, 本地缓存)
    data_t* tmpImgp,         // depth=17*17 (~1.1 KB, BRAM)
    
    // AXI-Lite 参数
    int Lx, int Ly, int Nx, int Ny, int nk
) {
    #pragma HLS INTERFACE m_axi port=mskf   bundle=gmem0 depth=262144
    #pragma HLS INTERFACE m_axi port=krns   bundle=gmem1 depth=1296    // 16*9*9
    #pragma HLS INTERFACE m_axi port=scales  bundle=gmem2 depth=16
    #pragma HLS INTERFACE m_axi port=tmpImgp bundle=gmem3 depth=289      // 17*17
    #pragma HLS INTERFACE s_axilite port=Lx   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Ly   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Nx   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Ny   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=nk   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl
    
    calc_socs_core(mskf, krns, scales, tmpImgp, Lx, Ly, Nx, Ny, nk);
}
```

**方案B（目标配置 Nx=16）**：
```cpp
void socs_top(
    // AXI-MM 输入/输出
    cmpxData_t* mskf,        // depth=512*512 (2 MB, DDR访问)
    cmpxData_t* krns,        // depth=16*33*33 (~70 KB, 可缓存)
    data_t* scales,          // depth=16 (64 B, 本地缓存)
    data_t* tmpImgp,         // depth=65*65 (16.9 KB, BRAM)
    
    // AXI-Lite 参数
    int Lx, int Ly, int Nx, int Ny, int nk
) {
    #pragma HLS INTERFACE m_axi port=mskf   bundle=gmem0 depth=262144
    #pragma HLS INTERFACE m_axi port=krns   bundle=gmem1 depth=17424   // 16*33*33
    #pragma HLS INTERFACE m_axi port=scales  bundle=gmem2 depth=16
    #pragma HLS INTERFACE m_axi port=tmpImgp bundle=gmem3 depth=4225     // 65*65
    #pragma HLS INTERFACE s_axilite port=Lx   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Ly   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Nx   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=Ny   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=nk   bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl
    
    calc_socs_core(mskf, krns, scales, tmpImgp, Lx, Ly, Nx, Ny, nk);
}
```

**接口说明**：
- 第一版采用 **AXI-MM + AXI-Lite** 标准接口
- `mskf` 是大数组（2 MB），主方案不放片上 BRAM，通过 AXI-MM 访问 DDR
- `krns` 和 `scales` 可考虑本地缓存
- `tmpImgp` 输出为 65×65 float，大小约 16.9 KB
- 该接口面向 DDR/全局存储器访问，便于后续平台集成和软件驱动

### C Simulation Testbench

```cpp
int main() {
    // 方案A：当前配置 (Nx=4)
    cmpxData_t mskf[512*512];
    cmpxData_t krns[16*9*9];         // 9×9 kernel
    data_t scales[16];
    data_t golden_tmpImgp[17*17];   // 17×17 output
    
    load_test_data(mskf, krns, scales, golden_tmpImgp);
    // golden_tmpImgp 来自 tmpImgp_pad32.bin
    
    data_t hls_tmpImgp[17*17];
    socs_top(mskf, krns, scales, hls_tmpImgp, 512, 512, 4, 4, 16);
    
    bool pass = verify_output(hls_tmpImgp, golden_tmpImgp, 17, 17);
    return pass ? 0 : 1;
    
    // 方案B：目标配置 (Nx=16)
    // cmpxData_t krns[16*33*33];        // 33×33 kernel
    // data_t golden_tmpImgp[65*65];     // 65×65 output
    // load_test_data(mskf, krns, scales, golden_tmpImgp);
    // // golden_tmpImgp 来自 tmpImgp_pad128.bin
    // 
    // data_t hls_tmpImgp[65*65];
    // socs_top(mskf, krns, scales, hls_tmpImgp, 512, 512, 16, 16, 16);
    // 
    // bool pass = verify_output(hls_tmpImgp, golden_tmpImgp, 65, 65);
    // return pass ? 0 : 1;
}
```

---

## 阶段 5：优化与综合

**目标**：性能优化并导出 RTL。

### Pipeline 与存储优化

- [ ] **局部循环优化**
  - 对 `nk` 循环内的各步骤做 `pipeline`
  - 对 `build_padded_ifft_input` 中的点乘循环优化
  
- [ ] **存储优化**
  - 对 `krns` 和 `scales` 尽量本地缓存
  - 对 `transpose` 过程使用 BRAM 分区：`ARRAY_PARTITION cyclic factor=4`
  - 对累加过程优化存储访问

- [ ] **Transpose 优化**
  - 第一版：朴素 BRAM 数组实现
  - 后续：Line Buffer 优化

### 性能目标

**方案A（当前配置 Nx=4）**：
- **初版目标**：< 1M cycles
- **计算依据**：
  - 单个 32 点 1D FFT ≈ 200 cycles（理想 pipeline）
  - 2D FFT 32×32 ≈ 12.8k cycles
  - 16 个 kernel × 12.8k ≈ 205k cycles（仅 IFFT 部分）
  - 加上点乘、累加开销 ≈ 500k cycles

**方案B（目标配置 Nx=16）**：
- **初版目标**：< 10M cycles
- **优化目标**：< 5M cycles
- **计算依据**：
  - 单个 128 点 1D FFT ≈ 900 cycles（理想 pipeline）
  - 2D FFT 128×128 ≈ 230k cycles
  - 16 个 kernel × 230k ≈ 3.7M cycles（仅 IFFT 部分）
  - 加上点乘、累加开销 ≈ 5M cycles

### 资源估算

- DSP: 主要 FFT 消耗，预估 ~200-400 DSP
- BRAM: ~50-100 BRAM (36Kb)
- **最终以 csynth 报告为准**

### 执行流程

- [ ] **C 综合**
  ```bash
  cd source/SOCS_HLS
  vitis-run --mode hls --tcl script/run_csynth.tcl
  ```

- [ ] **CoSim 验证**
  ```bash
  vitis-run --mode hls --cosim --config script/config/hls_config.cfg
  ```

- [ ] **导出 RTL 包**
  ```bash
  vitis-run --mode hls --package --config script/config/hls_config.cfg
  ```

---

## 验收标准

### A. HLS 实现正确性验收

| 验收项 | 方案A标准 | 方案B标准 |
|--------|-----------|-----------|
| 1D FFT/2D IFFT | 32点 FFT与CPU参考对齐，误差 `1e-5 ~ 1e-4` | 128点 FFT与CPU参考对齐，误差 `1e-5 ~ 1e-4` |
| `tmpImgp` 精度 | 与 `tmpImgp_pad32.bin`（17×17）误差达标（RMSE < 1e-4）| 与 `tmpImgp_pad128.bin`（65×65）误差达标（RMSE < 1e-4）|
| C Simulation | PASS | PASS |
| CoSim | PASS | PASS |
| RTL 导出 | 成功生成 RTL 包 | 成功生成 RTL 包 |

### B. litho.cpp 实现与原始算法对比

| 对比项 | 方案A说明 | 方案B说明 |
|--------|-----------|-----------|
| litho.cpp vs 原始 klitho_socs.cpp | litho.cpp 已使用 `nextPowerOfTwo(17)=32`，满足 2^N 标准 | litho.cpp 已使用 `nextPowerOfTwo(65)=128`，满足 2^N 标准 |
| 验收决策 | litho.cpp 当前实现即为改写版，可直接作为 golden | 同样满足 2^N 标准，无需额外改写 |
| 原始算法评估 | 可选：对比 17×17 vs 理论非填充版本（仅用于学术分析）| 可选：对比 65×65 vs 理论非填充版本（仅用于学术分析）|

---

## 关键风险与应对

### 风险 1：FFT 精度

**应对**：
- 阶段 1 单独验证 FFT 精度
- 使用 float 全浮点降低精度风险
- 记录实际误差，目标 `1e-5 ~ 1e-4` 量级

### 风险 2：改写版算法定义不一致

**应对**：
- 阶段 -1 先固化改写版 CPU reference
- `build_padded_ifft_input` 和 `extract_valid_65x65` 必须文档化
- HLS 实现严格遵循改写版定义

### 风险 3：片上存储瓶颈

**应对**：
- `mskf` 通过 AXI-MM 访问 DDR，不放片上
- `tmpImgp[65×65]` 可放入 BRAM（16.9 KB）
- 临时 IFFT 缓冲（128×128 complex ≈ 128 KB）通过 BRAM 分区优化

### 风险 4：性能不达标

**应对**：
- 初版目标 < 10M cycles（保守）
- 后续通过 pipeline、存储优化达到 < 5M cycles
- 最终以 csynth 报告为准

---

## 进度追踪

| 阶段 | 预计时间 | 状态 |
|------|----------|------|
| 阶段 -1：改写版算法基线确认 | 第0.5周 | 待启动 |
| 阶段 0：基础设施 + Golden 数据 | 第1周 | 待启动 |
| 阶段 1：1D FFT 验证 | 第2周 | 待启动 |
| 阶段 2：2D IFFT 验证 | 第3周 | 待启动 |
| 阶段 3：calcSOCS 核心实现 | 第4周 | 待启动 |
| 阶段 4：Top 接口 + Testbench | 第5周 | 待启动 |
| 阶段 5：优化 + 综合 | 第6周 | 待启动 |
| 阶段 6：FI 集成（可选） | 第7周+ | 待评估 |

---

## 参考资源

| 参考项 | 路径 | 用途 |
|--------|------|------|
| **当前 CPU 实现** | `verification/src/litho.cpp` | **实际运行版本**，**17×17 → 32×32 zero-padded IFFT**（已满足 2^N 标准） |
| 原始 SOCS 参考 | `reference/CPP_reference/Litho-SOCS/klitho_socs.cpp` | 原始算法逻辑 |
| HLS FFT 参考 | `reference/vitis_hls_ftt的实现/interface_stream/` | hls::fft 模板（注意路径修正） |
| 命令参考 | `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow.md` | vitis-run |
| **Golden 输入** | `output/verification/mskf_r.bin, mskf_i.bin, scales.bin` | **从 run_verification.py 生成** |
| **Golden 参考** | `output/verification/aerial_image_tcc_direct.bin` | **TCC 直接成像（理论标准）** |
| 配置参数 | `input/config/config.json` | 参数定义（**Nx/Ny 动态计算，非固定**） |

---

**修订日期**: 2026-04-07  
**修订依据**: litho.cpp 实际实现核查，发现已满足 2^N 标准（17×17 → 32×32）  
**核心变更**: 
- ✅ litho.cpp 已使用 `nextPowerOfTwo()` 自动填充到 2^N，无需额外改写
- 新增方案A/B配置路径选择
- 方案A（推荐）：当前配置 Nx=4，32×32 IFFT
- 方案B（目标）：需修改 config.json，Nx=16，128×128 IFFT