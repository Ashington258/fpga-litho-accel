#!/usr/bin/env python3
"""
HLS SOCS Golden Data Analysis - Simplified Version
===================================================

分析当前golden数据情况，识别HLS验证所需数据
"""

import os
import json
import numpy as np
from pathlib import Path

PROJECT_ROOT = Path("/root/project/FPGA-Litho")
VERIFICATION_DIR = PROJECT_ROOT / "output/verification"
DATA_DIR = PROJECT_ROOT / "source/SOCS_HLS/data"

def analyze_config():
    """分析当前配置和Nx"""
    config_file = PROJECT_ROOT / "input/config/config.json"
    with open(config_file) as f:
        config = json.load(f)
    
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
        outerSigma = 0.9
    
    # 计算Nx, Ny
    Nx = int(np.floor(NA * Lx * (1 + outerSigma) / wavelength))
    Ny = int(np.floor(NA * Ly * (1 + outerSigma) / wavelength))
    
    # 计算各尺寸
    convX = 4 * Nx + 1  # 物理卷积输出尺寸
    convY = 4 * Ny + 1
    fftConvX = int(2 ** np.ceil(np.log2(convX)))  # nextPowerOfTwo
    fftConvY = int(2 ** np.ceil(np.log2(convY)))
    kerX = 2 * Nx + 1  # SOCS核尺寸
    kerY = 2 * Ny + 1
    
    print("\n[CONFIG ANALYSIS]")
    print(f"  Lx={Lx}, Ly={Ly}, NA={NA}, λ={wavelength}, σ_outer={outerSigma}")
    print(f"  Nx={Nx}, Ny={Ny} (动态计算)")
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
    """检查现有golden数据"""
    print("\n[GOLDEN DATA CHECK]")
    
    required_files = {
        'mskf_r.bin': f"mask频域实部",
        'mskf_i.bin': f"mask频域虚部",
        'scales.bin': f"特征值权重",
        'aerial_image_socs_kernel.bin': f"SOCS最终输出",
        'kernels/krn_0_r.bin': f"第一个SOCS核实部",
        'kernels/krn_0_i.bin': f"第一个SOCS核虚部",
    }
    
    all_exist = True
    for fname, desc in required_files.items():
        fpath = VERIFICATION_DIR / fname
        if fpath.exists():
            size = fpath.stat().st_size
            print(f"  ✓ {fname}: {size} bytes ({desc})")
        else:
            print(f"  ✗ {fname}: MISSING ({desc})")
            all_exist = False
    
    return all_exist

def analyze_missing_data():
    """分析缺失的HLS所需数据"""
    print("\n[CRITICAL MISSING DATA]")
    print("=" * 70)
    print("⚠️  tmpImgp_pad32.bin (17×17 float) - HLS的直接输出目标")
    print("    当前litho.cpp输出完整512×512 aerial image")
    print("    需要提取calcSOCS中间步骤的tmpImgp")
    print()
    print("解决方案：")
    print("  方案A：修改litho.cpp添加中间输出（快速，但不推荐）")
    print("  方案B：编写独立calcSOCS_reference.cpp（推荐，清晰分离）")
    print()
    print("推荐方案B的原因：")
    print("  1. 不影响现有验证流程")
    print("  2. 便于HLS验证和维护")
    print("  3. 可文档化算法细节")

def main():
    print("HLS SOCS Golden Data Analysis")
    print("=" * 70)
    
    # 1. 分析配置
    params = analyze_config()
    
    # 2. 检查现有数据
    has_golden = check_existing_golden()
    
    # 3. 分析缺失数据
    analyze_missing_data()
    
    # 4. 总结
    print("\n[SUMMARY]")
    print("=" * 70)
    print(f"当前配置: Nx={params['Nx']}, Ny={params['Ny']}")
    print(f"IFFT目标: {params['fftConvX']}×{params['fftConvY']} zero-padded")
    print(f"输出尺寸: {params['convX']}×{params['convY']} (17×17)")
    print()
    print("已有golden: mask频域、特征值、SOCS核、最终输出")
    print("缺失golden: tmpImgp_pad32.bin (HLS直接输出)")
    print()
    print("下一步：编写calcSOCS_reference.cpp生成tmpImgp_pad32.bin")
    
    return True

if __name__ == "__main__":
    main()