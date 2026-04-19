# HLS SOCS Golden Data Extraction Plan

## 执行摘要

**问题**: 当前litho.cpp输出完整的512×512 aerial image，但HLS SOCS IP需要验证的是中间输出tmpImgp[17×17]。

**解决方案**: 编写独立的calcSOCS_reference.cpp程序，生成tmpImgp_pad32.bin作为HLS唯一golden。

**预计时间**: 4-6小时（编写reference + 文档化 + 验证）

---

## 1. 当前配置分析（已确认）

基于 `input/config/config.json` 和 `validation/golden/run_verification.py` 分析：

### 1.1 关键参数

| 参数       | 值      | 说明                                                                                          |
| ---------- | ------- | --------------------------------------------------------------------------------------------- |
| Lx, Ly     | 512×512 | Mask物理尺寸                                                                                  |
| NA         | 0.8     | 数值孔径                                                                                      |
| wavelength | 193 nm  | 波长                                                                                          |
| outerSigma | 0.9     | Annular source外半径                                                                          |
| **Nx, Ny** | **4×4** | **动态计算**: $N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$ |
| nk         | 10      | SOCS核数量（从TCC分解）                                                                       |

### 1.2 尺寸推导

| 尺寸类型     | 值        | 计算公式           |
| ------------ | --------- | ------------------ |
| 物理卷积尺寸 | **17×17** | convX = 4×Nx+1     |
| IFFT执行尺寸 | **32×32** | nextPowerOfTwo(17) |
| SOCS核尺寸   | **9×9**   | kerX = 2×Nx+1      |

**关键发现**: litho.cpp已满足2^N标准（17→32 zero-padded），无需额外改写！

---

## 2. 已存在Golden数据（已验证）

通过 `output/verification/` 目录检查：

### 2.1 输入数据

✅ **mskf_r.bin, mskf_i.bin** (512×512 complex<float>)
- Mask频域数据，已通过FFT计算
- 大小: 1048576 bytes each (512×512×4)
- 来源: `validation/golden/src/litho.cpp` → FT_r2c_padded()

✅ **scales.bin** (nk=10个float)
- SOCS特征值权重
- 大小: 40 bytes (10×4)
- 来源: TCC矩阵的eigenvalue decomposition

✅ **kernels/krn_k_r.bin, krn_k_i.bin** (10个核，每个9×9 complex<float>)
- SOCS频域核
- 大小: 324 bytes each (9×9×4)
- 来源: TCC分解的eigenvector

### 2.2 输出数据

✅ **aerial_image_socs_kernel.bin** (512×512 float)
- SOCS方法的最终输出
- 大小: 1048576 bytes (512×512×4)
- 包含完整的FI插值和crop

---

## 3. 缺失Golden数据（关键）

⚠️ **tmpImgp_pad32.bin** (17×17 float)

### 3.1 为什么缺失？

当前 `calcSOCS()` 函数流程：
```
calcSOCS() {
    1. 频域点乘: kernel[k] * mskf (中心区域)
    2. 补零嵌入: → padded[32×32]
    3. 2D IFFT: → spatial amplitude[32×32]
    4. 强度累加: tmpImg[32×32] += scale[k] * (re² + im²)
    5. **fftshift**: tmpImgp[32×32]  ← HLS需要这个！
    6. Fourier Interpolation: → padded[512×512]
    7. Center crop: → final image[512×512]
}
```

litho.cpp输出的是步骤7的结果（完整512×512），但HLS IP只负责步骤1-5，输出应为tmpImgp[17×17]。

### 3.2 HLS IP的正确输出

HLS SOCS IP应输出：
- **tmpImgp[17×17]** - 从32×32中提取的中心17×17区域
- 不包括FI插值（留在Host端）

---

## 4. 解决方案：编写calcSOCS_reference.cpp

### 4.1 方案对比

| 方案                 | 优点                | 缺点         | 推荐 |
| -------------------- | ------------------- | ------------ | ---- |
| A: 修改litho.cpp     | 快速（2小时）       | 影响验证流程 | ❌    |
| B: 编写独立reference | 清晰分离（4-6小时） | 需额外开发   | ✅    |

**推荐方案B**：编写独立的 `calcSOCS_reference.cpp`，原因：
1. 不影响现有验证流程
2. 可文档化算法细节
3. 便于HLS验证和维护

### 4.2 calcSOCS_reference.cpp设计

**文件路径**: `validation/golden/src/calcSOCS_reference.cpp`

**输入**:
- mskf_r.bin, mskf_i.bin (512×512)
- scales.bin (10个eigenvalue)
- kernels/krn_k_r/i.bin (10个核，9×9)

**输出**:
- **tmpImgp_pad32.bin** (17×17 float) - **HLS唯一golden**
- tmpImg_full.bin (32×32 float) - 用于调试（可选）

**算法流程**（严格复现litho.cpp的calcSOCS步骤1-5）:

```cpp
void calcSOCS_reference() {
    // 1. 加载输入数据
    load_mskf("output/verification/mskf_r.bin", "mskf_i.bin");
    load_scales("scales.bin");
    load_kernels("kernels/krn_*_r.bin", "krn_*_i.bin");
    
    // 2. 初始化累加器
    vector<double> tmpImg(32*32, 0.0);
    
    // 3. 对每个kernel处理
    for (int k = 0; k < 10; k++) {
        // 3.1 频域点乘 + 补零嵌入
        vector<ComplexD> padded(32*32, 0);
        embed_kernel_mask_product(padded, kernels[k], mskf, Nx=4, Ny=4);
        
        // 3.2 2D IFFT (32×32)
        vector<ComplexD> spatial(32*32);
        ifft2d_32x32(padded, spatial);
        
        // 3.3 强度累加
        for (int i = 0; i < 32*32; i++) {
            double re = spatial[i].real();
            double im = spatial[i].imag();
            tmpImg[i] += scales[k] * (re*re + im*im);
        }
    }
    
    // 4. fftshift (32×32 → 32×32)
    vector<double> tmpImgp(32*32);
    myShift(tmpImg, tmpImgp, 32, 32, true, true);
    
    // 5. 提取中心17×17区域
    vector<float> tmpImgp_17x17(17*17);
    extract_center_17x17(tmpImgp, tmpImgp_17x17);
    
    // 6. 输出golden
    writeFloatArrayToBinary("tmpImgp_pad32.bin", tmpImgp_17x17, 17*17);
}
```

---

## 5. 关键算法细节（必须文档化）

### 5.1 补零布局规则（embed_kernel_mask_product）

**litho.cpp当前使用**：
```cpp
// dif偏移（bottom-right embedding）
int difX = fftConvX - kerX;  // = 32 - 9 = 23
int difY = fftConvY - kerY;  // = 32 - 9 = 23

// 嵌入位置
int by = difY + ky;  // ky: 0~8 (kernel y索引)
int bx = difX + kx;  // kx: 0~8 (kernel x索引)
```

**问题**: 这种"bottom-right"布局可能导致相位不对齐

**HLS建议使用**（centered embedding）：
```cpp
// off偏移（centered embedding）- 推荐
int offX = (fftConvX - kerX) / 2;  // = (32-9)/2 = 11
int offY = (fftConvY - kerY) / 2;  // = (32-9)/2 = 11

// 嵌入位置
int by = offY + ky;
int bx = offX + kx;
```

**决策**: calcSOCS_reference需同时实现两种布局，输出两个版本golden，对比评估。

### 5.2 IFFT缩放约定

**FFTW BACKWARD**:
- 默认不缩放，输出放大N倍
- 需手动除以 (fftSizeX × fftSizeY)

**HLS FFT**:
- 需确认scaling规则（可能在IP内部已处理）

**calcSOCS_reference必须**:
```cpp
// 执行IFFT后
for (int i = 0; i < 32*32; i++) {
    spatial[i] /= (32.0 * 32.0);  // 手动缩放
}
```

### 5.3 提取规则（extract_center_17x17）

从32×32 tmpImgp中提取中心17×17：

```cpp
void extract_center_17x17(
    vector<double>& in,     // 32×32
    vector<float>& out      // 17×17
) {
    int offsetX = (32 - 17) / 2;  // = 7
    int offsetY = (32 - 17) / 2;  // = 7
    
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 17; x++) {
            int srcY = offsetY + y;
            int srcX = offsetX + x;
            out[y * 17 + x] = static_cast<float>(in[srcY * 32 + srcX]);
        }
    }
}
```

---

## 6. Golden数据验证流程

获得tmpImgp_pad32.bin后的验证流程：

### 6.1 C Simulation验证

```cpp
// HLS testbench
int main() {
    // 加载golden
    float golden_tmpImgp[17*17];
    load_golden("tmpImgp_pad32.bin", golden_tmpImgp);
    
    // 加载输入
    ComplexF mskf[512*512];
    ComplexF krns[10*9*9];
    float scales[10];
    load_inputs(mskf, krns, scales);
    
    // 执行HLS SOCS
    float hls_tmpImgp[17*17];
    socs_top(mskf, krns, scales, hls_tmpImgp, 512, 512, 4, 4, 10);
    
    // 验证
    bool pass = verify_output(hls_tmpImgp, golden_tmpImgp, 17, 17);
    float max_error = compute_max_error(hls_tmpImgp, golden_tmpImgp);
    
    cout << "Max Error: " << max_error << endl;
    cout << "PASS: " << pass << endl;
    
    return pass ? 0 : 1;
}
```

### 6.2 验收标准

| 验收项       | 标准     |
| ------------ | -------- |
| 最大绝对误差 | < 1e-5   |
| 归一化RMSE   | < 1e-4   |
| 峰值位置     | 完全一致 |
| 数值范围     | 相同量级 |

---

## 7. 补充修正HLS Skill

基于以上分析，需修正已创建的三个skill：

### 7.1 hls-csynth-verify修正点

添加：
- Nx/Ny动态计算说明（已在description中）
- 当前配置：Nx=4, Ny=4
- IFFT尺寸：32×32（方案A）

### 7.2 hls-full-validation修正点

添加：
- Golden数据生成前置步骤：运行calcSOCS_reference
- 明确tmpImgp_pad32.bin作为唯一golden

### 7.3 hls-board-validation修正点

添加：
- 输出buffer长度调整：
  - 方案A: len=289 (17×17)
  - 方案B: len=4225 (65×65)

---

## 8. 执行计划

### 8.1 立即执行

1. **编写calcSOCS_reference.cpp**（4-6小时）
   - 复现litho.cpp的calcSOCS算法（步骤1-5）
   - 实现两种补零布局（dif和off）
   - 输出tmpImgp_pad32.bin（17×17）

2. **运行生成golden**
   ```bash
   cd validation/golden/src
   make calcSOCS_reference
   ./calcSOCS_reference
   ```

3. **验证golden正确性**
   - 对比tmpImgp_pad32与aerial_image_socs_kernel的对应区域
   - 量化评估算法改写偏差

### 8.2 后续HLS开发

1. **阶段0**: 准备HLS基础设施和golden数据
2. **阶段1**: 验证32点FFT可用性
3. **阶段2**: 实现32×32 2D IFFT
4. **阶段3**: 完整calcSOCS实现
5. **阶段4**: Top接口和testbench
6. **阶段5**: 优化和综合

---

## 9. 总结

**当前状态**:
- ✅ 已有大部分golden数据（mskf, scales, kernels）
- ⚠️ 缺失关键tmpImgp_pad32.bin

**解决方案**:
- 编写calcSOCS_reference.cpp生成唯一golden

**关键发现**:
- litho.cpp已满足2^N标准（17→32），无需额外改写
- 当前配置Nx=4（而非Nx=16），IFFT尺寸32×32

**下一步**:
- 立即执行calcSOCS_reference.cpp编写
- 补充修正HLS skill文档
