#!/usr/bin/env python3
"""
HLS SOCS Golden Data Extractor
===============================
从litho.cpp运行结果中提取HLS验证所需的golden数据

根据SOCS_TODO.md，HLS IP需要验证的是：
- 方案A（当前配置）：32×32 zero-padded IFFT → 17×17 tmpImgp输出
- 方案B（目标配置）：128×128 zero-padded IFFT → 65×65 tmpImgp输出

当前litho.cpp输出的是完整的512×512 aerial_image，需要提取中间步骤的tmpImgp。
"""

import os
import sys
import numpy as np
from pathlib import Path
import subprocess

PROJECT_ROOT = Path("/root/project/FPGA-Litho")
VERIFICATION_DIR = PROJECT_ROOT / "output/verification"
SOCS_HLS_DIR = PROJECT_ROOT / "source/SOCS_HLS"
DATA_DIR = SOCS_HLS_DIR / "data"

def analyze_config_and_nx():
    """分析当前配置和计算Nx"""
    import json
    
    config_path = PROJECT_ROOT / "input/config/config.json"
    with open(config_path) as f:
        config = json.load(f)
    
    # 提取关键参数
    Lx = config['mask']['period']['Lx']
    Ly = config['mask']['period']['Ly']
    NA = config['optics']['NA']
    wavelength = config['optics']['wavelength']
    
    # 计算outerSigma
    source_type = config['source']['type']
    if source_type == 'Annular':
        outerSigma = config['source']['annular']['outerRadius']
    elif source_type == 'Dipole':
        outerSigma = config['source']['dipole']['radius']
    else:
        outerSigma = 1.0
    
    # 计算Nx
    Nx = int(np.floor(NA * Lx * (1 + outerSigma) / wavelength))
    Ny = int(np.floor(NA * Ly * (1 + outerSigma) / wavelength))
    
    # 计算关键尺寸
    convX = 4 * Nx + 1  # 物理卷积输出尺寸
    fftConvX = int(2 ** np.ceil(np.log2(convX)))  # nextPowerOfTwo
    kerX = 2 * Nx + 1   # SOCS核尺寸
    
    print(f"[CONFIG ANALYSIS]")
    print(f"  Lx={Lx}, Ly={Ly}, NA={NA}, λ={wavelength}, σ_outer={outerSigma}")
    print(f"  Nx={Nx}, Ny={Ny}")
    print(f"  Physical convolution size: {convX}×{convY}")
    print(f"  IFFT execution size: {fftConvX}×{fftConvY} (zero-padded)")
    print(f"  Kernel support size: {kerX}×{kerY}")
    
    return {
        'Lx': Lx, 'Ly': Ly, 'NA': NA, 'wavelength': wavelength, 'outerSigma': outerSigma,
        'Nx': Nx, 'Ny': Ny,
        'convX': convX, 'convY': convY,
        'fftConvX': fftConvX, 'fftConvY': fftConvY,
        'kerX': kerX, 'kerY': kerY
    }

def check_existing_golden():
    """检查已存在的golden数据"""
    print(f"\n[GOLDEN DATA CHECK]")
    
    required_files = [
        'mskf_r.bin', 'mskf_i.bin',    # mask频域 (512×512)
        'scales.bin',                   # 特征值 (nk个)
        'kernels/krn_0_r.bin',         # SOCS核示例
        'aerial_image_socs_kernel.bin'  # SOCS成像结果 (512×512)
    ]
    
    all_exist = True
    for fname in required_files:
        fpath = VERIFICATION_DIR / fname
        if fpath.exists():
            size = fpath.stat().st_size
            print(f"  ✓ {fname} ({size} bytes)")
        else:
            print(f"  ✗ {fname} MISSING")
            all_exist = False
    
    return all_exist

def extract_socs_golden(params):
    """
    提取HLS SOCS验证所需的golden数据
    
    关键问题：当前litho.cpp输出的是完整512×512 image，不是中间17×17 tmpImgp
    需要修改litho.cpp添加tmpImgp输出，或重新实现改写版CPU reference
    """
    print(f"\n[GOLDEN EXTRACTION]")
    
    # 创建数据目录
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    
    # 1. 复制mask频域数据
    mskf_r = np.fromfile(VERIFICATION_DIR / 'mskf_r.bin', dtype=np.float32)
    mskf_i = np.fromfile(VERIFICATION_DIR / 'mskf_i.bin', dtype=np.float32)
    print(f"  ✓ Mask频域: {mskf_r.shape} (expected {params['Lx']*params['Ly']})")
    
    # 验证尺寸
    expected_mskf_size = params['Lx'] * params['Ly']
    if mskf_r.size != expected_mskf_size:
        print(f"  ⚠ WARNING: mskf size mismatch!")
    
    # 2. 复制特征值
    scales = np.fromfile(VERIFICATION_DIR / 'scales.bin', dtype=np.float32)
    print(f"  ✓ Scales: {scales.size} kernels")
    print(f"    Eigenvalues: {scales[:5]}...")
    
    # 3. 加载所有SOCS核
    kernel_size = params['kerX'] * params['kerY']
    nk = scales.size
    
    krns_complex = []
    for k in range(nk):
        krn_r = np.fromfile(VERIFICATION_DIR / f'kernels/krn_{k}_r.bin', dtype=np.float32)
        krn_i = np.fromfile(VERIFICATION_DIR / f'kernels/krn_{k}_i.bin', dtype=np.float32)
        
        # 验证核尺寸
        if krn_r.size != kernel_size:
            print(f"  ⚠ WARNING: kernel {k} size mismatch!")
        
        # 构造复数核
        krn_complex = krn_r + 1j * krn_i
        krns_complex.append(krn_complex)
    
    print(f"  ✓ SOCS kernels: {nk} kernels, each {params['kerX']}×{params['kerY']}")
    
    # 4. 问题：缺少tmpImgp[17×17] golden
    print(f"\n  ⚠ CRITICAL: Missing tmpImgp golden data!")
    print(f"    Current litho.cpp outputs final 512×512 image, not intermediate 17×17 tmpImgp")
    print(f"    HLS IP target: {params['fftConvX']}×{params['fftConvY']} IFFT → {params['convX']}×{params['convY']} output")
    
    return False

def propose_solution(params):
    """
    提出解决方案：需要修改litho.cpp或编写改写版CPU reference
    
    方案A：修改litho.cpp添加tmpImgp输出
    方案B：编写独立的改写版CPU reference（推荐）
    """
    print(f"\n[PROPOSED SOLUTION]")
    print("=" * 70)
    
    print("方案A：修改litho.cpp添加中间输出（简单，但不推荐）")
    print("  - 在calcSOCS函数中添加tmpImgp输出")
    print("  - 修改writeFloatArrayToBinary调用")
    print("  - 输出文件：tmpImgp_pad32.bin (17×17 float)")
    print("  - 缺点：修改验证程序，可能影响其他流程")
    
    print(f"\n方案B：编写独立改写版CPU reference（推荐）")
    print("  - 创建独立的calcSOCS_reference.cpp")
    print("  - 严格实现HLS目标算法：32×32 zero-padded IFFT")
    print("  - 输出tmpImgp_pad32.bin作为唯一HLS golden")
    print("  - 优点：清晰分离，便于HLS验证和维护")
    
    print(f"\n关键算法细节（需要文档化）：")
    print(f"  1. build_padded_ifft_input布局规则：")
    print(f"     - kernel*mask product嵌入到{params['fftConvX']}×{params['fftConvY']}数组")
    print(f"     - litho.cpp当前使用'dif'偏移（bottom-right embedding）")
    print(f"     - 建议HLS使用'off'偏移（centered embedding，相位对齐更好）")
    
    print(f"  2. extract_valid_output提取规则：")
    print(f"     - 从{params['fftConvX']}×{params['fftConvY']} IFFT输出提取{params['convX']}×{params['convY']}")
    print(f"     - 需要明确定义提取区域offset")
    
    print(f"  3. FFT normalization convention：")
    print(f"     - FFTW BACKWARD（IFFT）默认不缩放")
    print(f"     - HLS FFT可能需要手动缩放 1/N")
    
    print(f"\n下一步行动：")
    print(f"  1. 创建calcSOCS_reference.cpp（方案B）")
    print(f"  2. 文档化补零布局和提取规则")
    print(f"  3. 生成tmpImgp_pad32.bin作为HLS唯一golden")
    print(f"  4. 运行HLS验证流程")

def create_golden_extraction_plan():
    """创建详细的golden数据提取计划"""
    
    plan_file = DATA_DIR / "GOLDEN_EXTRACTION_PLAN.md"
    
    plan_content = f"""# HLS SOCS Golden Data Extraction Plan

## 当前状态分析

基于配置文件和litho.cpp分析：

### 关键参数（当前配置）
- **Lx, Ly**: 512×512
- **NA**: 0.8
- **wavelength**: 193 nm
- **outerSigma**: 0.9 (Annular source)
- **Nx, Ny**: 4×4（动态计算）
- **物理卷积尺寸**: 17×17
- **IFFT执行尺寸**: 32×32（zero-padded）
- **SOCS核尺寸**: 9×9

### 已存在的Golden数据
✓ mskf_r.bin, mskf_i.bin (mask频域，512×512 complex)
✓ scales.bin (特征值，nk=10)
✓ kernels/krn_k_r.bin, krn_k_i.bin (10个SOCS核，每个9×9)
✓ aerial_image_socs_kernel.bin (最终输出，512×512)

### 缺失的Golden数据
⚠ **tmpImgp_pad32.bin** (17×17 float)
  - 这是HLS IP的直接输出目标
  - 当前litho.cpp输出的是完整512×512 image
  - 需要提取calcSOCS函数中的中间步骤

## 解决方案

### 方案A：修改litho.cpp添加输出（不推荐）
优点：简单快速
缺点：修改验证程序，影响其他流程

修改点：
1. 在calcSOCS函数中，在Fourier Interpolation之前添加：
   ```cpp
   // 输出tmpImgp用于HLS验证
   vector<float> tmpImgp_float(fftConvX * fftConvY);
   for (int i = 0; i < fftConvX * fftConvY; i++) {
       tmpImgp_float[i] = static_cast<float>(tmpImgp[i]);
   }
   writeFloatArrayToBinary(outputDir + "/tmpImgp_pad32.bin", 
                           tmpImgp_float, fftConvX * fftConvY);
   ```

### 方案B：编写独立改写版CPU reference（推荐）
优点：清晰分离，便于HLS验证和维护

实现步骤：
1. 创建 `verification/src/calcSOCS_reference.cpp`
2. 从litho.cpp中提取calcSOCS核心逻辑
3. 输出tmpImgp_pad32.bin（17×17）作为唯一golden
4. 文档化补零布局和提取规则

关键算法细节：
- **build_padded_ifft_input**: 9×9 kernel*mask → 32×32 padded
  - 建议使用centered embedding（相位对齐）
  - 偏移：offX = (32-9)/2 = 11, offY = 11
  
- **IFFT execution**: FFTW BACKWARD，不缩放
  - HLS需要手动缩放1/(32×32)
  
- **extract_valid_output**: 32×32 → 17×17
  - 提取区域：center crop
  - 偏移：cropX = (32-17)/2 = 7, cropY = 7

## HLS验证流程

完成golden提取后的验证步骤：
1. **C Simulation**: tmpImgp_pad32.bin作为golden
2. **C Synthesis**: 检查Fmax/Latency指标
3. **Co-Simulation**: RTL与C结果对比
4. **Package**: 导出RTL/IP

## 时间估算

- 方案A：~2小时（修改+测试）
- 方案B：~4-6小时（编写reference+文档化+验证）

推荐：方案B，因为更清晰且可维护。
"""
    
    with open(plan_file, 'w') as f:
        f.write(plan_content)
    
    print(f"  ✓ Golden extraction plan saved: {plan_file}")
    return plan_file

def main():
    print("=" * 70)
    print("HLS SOCS Golden Data Extractor")
    print("=" * 70)
    
    # 1. 分析配置和Nx
    params = analyze_config_and_nx()
    
    # 2. 检查现有golden
    has_golden = check_existing_golden()
    
    if not has_golden:
        print("\n[ERROR] Missing required golden files. Run verification first:")
        print("  python verification/run_verification.py")
        return False
    
    # 3. 尝试提取golden（会失败，因为缺少tmpImgp）
    success = extract_socs_golden(params)
    
    # 4. 提出解决方案
    propose_solution(params)
    
    # 5. 创建详细计划文档
    plan_file = create_golden_extraction_plan()
    
    print(f"\n[SUMMARY]")
    print("=" * 70)
    print("当前状态：缺少关键golden数据 tmpImgp_pad32.bin")
    print(f"详细计划已保存：{plan_file}")
    print("\n推荐下一步：编写独立改写版CPU reference（方案B）")
    print("预计时间：4-6小时")
    
    return True

if __name__ == "__main__":
    main()