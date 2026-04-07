---
name: hls-golden-generation
description: 'WORKFLOW SKILL — Generate HLS golden data for SOCS IP validation. USE FOR: running verification scripts; extracting tmpImgp_pad32.bin intermediate output; creating calcSOCS_reference CPU implementation. Essential for establishing HLS validation baseline. DO NOT USE FOR: HLS synthesis; board testing; production deployment.'
---

# HLS Golden Data Generation Skill

## 适用场景

当用户需要执行以下任务时使用此skill:
- **生成HLS验证数据**: 从配置参数运行verification生成输入数据
- **提取中间输出**: 生成tmpImgp_pad32.bin作为HLS唯一golden
- **编写reference**: 创建calcSOCS_reference.cpp独立程序
- **验证数据正确性**: 对比golden与实际HLS输出

## 核心约束

**项目路径**: `/root/project/FPGA-Litho`

**关键参数（当前配置）**:
- **Nx, Ny**: 动态计算，当前值为4×4
- **计算公式**: $N_x = \lfloor \frac{NA \times L_x \times (1+\sigma_{outer})}{\lambda} \rfloor$
- **配置来源**: `input/config/config.json`
- **输出目录**: `output/verification/`

**Golden数据分类**:
1. **输入数据**: mskf, scales, kernels (已存在)
2. **中间输出**: tmpImgp_pad32.bin (缺失，需生成)
3. **最终输出**: aerial_image (已存在，用于端到端验证)

## 执行流程

### 步骤 1: 分析当前配置和Golden状态

**命令**:
```bash
cd /root/project/FPGA-Litho
python source/SOCS_HLS/scripts/extract_hls_golden_simple.py
```

**输出分析**:
- 当前配置参数（Nx, Ny, 尺寸）
- 已存在的golden数据清单
- 缺失数据识别

### 步骤 2: 运行基础Verification生成输入数据

**前提**: calcSOCS_reference尚未编写完成

**命令**:
```bash
cd /root/project/FPGA-Litho
python verification/run_verification.py
```

**生成的输入数据**:
- `mskf_r.bin`, `mskf_i.bin`: mask频域（512×512 complex<float>)
- `scales.bin`: 特征值权重（nk个float）
- `kernels/krn_k_r/i.bin`: SOCS核（nk个，每个9×9 complex<float>)
- `aerial_image_socs_kernel.bin`: 最终输出（512×512 float）

**验证生成成功**:
```bash
ls -lh output/verification/
# 检查文件大小
# mskf_r/i.bin: ~1MB each (512×512×4)
# scales.bin: 40 bytes (10×4 for nk=10)
# kernels: 324 bytes each (9×9×4)
```

### 步骤 3: 编写calcSOCS_reference生成中间输出

**关键问题**: litho.cpp输出完整512×512 aerial image，但HLS需要17×17 tmpImgp中间输出。

**解决方案**: 编写独立的calcSOCS_reference程序

**设计要点**:
1. **文件**: `verification/src/calcSOCS_reference.cpp`
2. **输入**: 使用步骤2生成的数据
3. **输出**: `tmpImgp_pad32.bin` (17×17 float)
4. **算法**: 严格复现litho.cpp的calcSOCS步骤1-5

**核心算法流程**:
```cpp
void calcSOCS_reference() {
    // 1. 加载输入
    load_mskf("output/verification/mskf_r.bin", "mskf_i.bin");
    load_scales("scales.bin");
    load_kernels("kernels/krn_*_r.bin", "krn_*_i.bin");
    
    // 2. 初始化累加器
    vector<double> tmpImg(32*32, 0.0);
    
    // 3. 对每个kernel处理
    for (int k = 0; k < nk; k++) {
        // 3.1 频域点乘 + 补零嵌入（关键步骤）
        vector<ComplexD> padded(32*32, 0);
        
        // 补零布局1：dif偏移（litho.cpp当前）
        int difX = 32 - 9;  // = 23
        int difY = 32 - 9;
        
        // 补零布局2：off偏移（HLS推荐）
        int offX = (32 - 9) / 2;  // = 11
        int offY = (32 - 9) / 2;
        
        embed_kernel_mask_product(padded, kernels[k], mskf, 
                                   Nx=4, Ny=4, layout="centered");
        
        // 3.2 2D IFFT (32×32)
        vector<ComplexD> spatial(32*32);
        ifft2d_32x32(padded, spatial);
        
        // 3.3 手动缩放（FFTW BACKWARD不自动缩放）
        for (int i = 0; i < 32*32; i++) {
            spatial[i] /= (32.0 * 32.0);
        }
        
        // 3.4 强度累加
        for (int i = 0; i < 32*32; i++) {
            double re = spatial[i].real();
            double im = spatial[i].imag();
            tmpImg[i] += scales[k] * (re*re + im*im);
        }
    }
    
    // 4. fftshift
    vector<double> tmpImgp(32*32);
    myShift(tmpImg, tmpImgp, 32, 32, true, true);
    
    // 5. 提取中心17×17
    vector<float> output(17*17);
    int offsetX = (32 - 17) / 2;  // = 7
    int offsetY = (32 - 17) / 2;
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 17; x++) {
            output[y*17 + x] = tmpImgp[(offsetY+y)*32 + (offsetX+x)];
        }
    }
    
    // 6. 输出golden
    writeFloatArrayToBinary("output/verification/tmpImgp_pad32.bin", 
                             output, 17*17);
}
```

**关键细节文档化**:
- 补零布局规则（dif vs off）
- IFFT缩放约定（手动除以N²）
- 提取规则（从32×32中心提取17×17）

### 步骤 4: 运行calcSOCS_reference生成Golden

**编译和运行**:
```bash
cd /root/project/FPGA-Litho/verification/src

# 编译（如果Makefile已配置）
make calcSOCS_reference

# 运行生成golden
./calcSOCS_reference
```

**验证生成成功**:
```bash
ls -lh output/verification/tmpImgp_pad32.bin
# 预期大小: 289 bytes (17×17×4)

# 检查数值范围
python -c "
import numpy as np
data = np.fromfile('output/verification/tmpImgp_pad32.bin', dtype=np.float32)
print(f'Shape: {data.shape}')
print(f'Min: {data.min()}, Max: {data.max()}, Mean: {data.mean()}')
"
```

### 步骤 5: 验证Golden正确性

**对比验证**（可选）:
```python
import numpy as np

# 加载golden中间输出
tmpImgp_17x17 = np.fromfile('output/verification/tmpImgp_pad32.bin', 
                              dtype=np.float32)

# 加载完整aerial image
aerial_full = np.fromfile('output/verification/aerial_image_socs_kernel.bin', 
                            dtype=np.float32)

# 提取对应区域（512×512的中心17×17，经过FI插值）
# 注意：这是端到端验证，tmpImgp需要经过FI才能匹配aerial_full

print("Golden intermediate output generated successfully!")
print("Ready for HLS C Simulation validation.")
```

## 行为规则

**自动化处理**:
1. 如果calcSOCS_reference未编写，提示用户或帮助编写
2. 如果基础verification未运行，先运行生成输入数据
3. 如果golden数据已存在，跳过生成步骤直接验证

**验证检查点**:
- 文件大小正确（17×17×4 = 289 bytes）
- 数值范围合理（非全零或异常值）
- 与完整aerial image逻辑一致

**失败处理**:
- 如果calcSOCS_reference编译失败，检查FFTW库依赖
- 如果数值异常，检查补零布局和IFFT缩放
- 如果与aerial不匹配，检查算法步骤是否正确复现

## 关键决策点

### 补零布局选择

**决策**: 使用哪种补零布局？

**方案1: dif偏移（bottom-right）**:
- litho.cpp当前使用
- 可能相位不对齐
- 与原始实现一致

**方案2: off偏移（centered）**:
- HLS推荐使用
- 相位对齐更好
- 需要验证与原始算法偏差

**建议**: 先生成两个版本对比评估，选择偏差最小的方案。

### IFFT缩放约定

**关键**: FFTW BACKWARD默认不缩放（输出放大N倍）

**必须**: 手动除以 (fftSizeX × fftSizeY)
```cpp
for (int i = 0; i < 32*32; i++) {
    spatial[i] /= (32.0 * 32.0);  // 1/1024
}
```

**HLS FFT**: 需确认scaling规则，可能IP内部已处理。

## 参考文档

**详细计划**: `source/SOCS_HLS/data/GOLDEN_EXTRACTION_PLAN.md`
- 完整的golden数据提取流程
- 算法细节文档化
- 验证验收标准

**相关程序**:
- `verification/run_verification.py`: 基础verification
- `verification/src/litho.cpp`: 完整实现（参考算法）
- `source/SOCS_HLS/scripts/extract_hls_golden_simple.py`: 配置分析

**输出数据**:
- 输入: `output/verification/mskf_r/i.bin`, `scales.bin`, `kernels/`
- 中间: `output/verification/tmpImgp_pad32.bin` (本skill的目标)
- 最终: `output/verification/aerial_image_socs_kernel.bin`

## 下一步建议

完成Golden生成后:
- **进行HLS C Simulation**: 用tmpImgp_pad32.bin验证算法正确性
- **调用hls-full-validation**: 执行完整HLS验证流程
- **对比补零布局**: 评估dif vs off两种方案的偏差