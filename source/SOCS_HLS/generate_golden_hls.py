#!/usr/bin/env python3
"""
SOCS HLS Golden数据生成脚本
从 verification 输出转换为 HLS 测试格式

用法：python generate_golden_hls.py
"""

import numpy as np
import os
import shutil
from pathlib import Path

# ============================================================================
# 配置路径
# ============================================================================

PROJECT_ROOT = Path("/root/project/FPGA-Litho")
VERIFICATION_OUTPUT = PROJECT_ROOT / "output" / "verification"
HLS_GOLDEN_DIR = PROJECT_ROOT / "source" / "SOCS_HLS" / "hls" / "tb" / "golden"

# ============================================================================
# 数据复制函数
# ============================================================================

def copy_verification_to_hls_golden():
    """将verification输出转换为HLS golden格式"""
    
    print("=" * 60)
    print("SOCS HLS Golden Data Generation")
    print("=" * 60)
    
    # 创建目标目录
    HLS_GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    
    # ============================================================================
    # 复制Mask频域数据
    # ============================================================================
    
    print("\n[1] Copying Mask Frequency Data...")
    
    mskf_r_src = VERIFICATION_OUTPUT / "mskf_r.bin"
    mskf_i_src = VERIFICATION_OUTPUT / "mskf_i.bin"
    
    if mskf_r_src.exists() and mskf_i_src.exists():
        shutil.copy(mskf_r_src, HLS_GOLDEN_DIR / "mskf_r.bin")
        shutil.copy(mskf_i_src, HLS_GOLDEN_DIR / "mskf_i.bin")
        
        # 验证数据
        mskf_r = np.fromfile(mskf_r_src, dtype=np.float32)
        mskf_i = np.fromfile(mskf_i_src, dtype=np.float32)
        
        print(f"  ✓ mskf_r.bin: {len(mskf_r)} floats")
        print(f"  ✓ mskf_i.bin: {len(mskf_i)} floats")
        print(f"  Shape: 512x512 (expected)")
    else:
        print("  ✗ ERROR: mskf files not found!")
        return False
    
    # ============================================================================
    # 复制SOCS核数据
    # ============================================================================
    
    print("\n[2] Copying SOCS Kernels...")
    
    kernels_src_dir = VERIFICATION_OUTPUT / "kernels"
    kernel_info_src = kernels_src_dir / "kernel_info.txt"
    
    if kernel_info_src.exists():
        shutil.copy(kernel_info_src, HLS_GOLDEN_DIR / "kernel_info.txt")
        
        # 解析核数量
        with open(kernel_info_src, 'r') as f:
            for line in f:
                if "Number of Kernels:" in line:
                    nk = int(line.split(':')[1].strip())
                    break
        
        print(f"  Number of kernels: {nk}")
        
        # 复制所有核文件
        for k in range(nk):
            krn_r_src = kernels_src_dir / f"krn_{k}_r.bin"
            krn_i_src = kernels_src_dir / f"krn_{k}_i.bin"
            
            if krn_r_src.exists() and krn_i_src.exists():
                shutil.copy(krn_r_src, HLS_GOLDEN_DIR / f"krn_{k}_r.bin")
                shutil.copy(krn_i_src, HLS_GOLDEN_DIR / f"krn_{k}_i.bin")
                
                # 验证数据
                krn_r = np.fromfile(krn_r_src, dtype=np.float32)
                print(f"  ✓ krn_{k}: {len(krn_r)} floats (9x9)")
            else:
                print(f"  ✗ ERROR: krn_{k} files not found!")
                return False
    else:
        print("  ✗ ERROR: kernel_info.txt not found!")
        return False
    
    # ============================================================================
    # 复制特征值
    # ============================================================================
    
    print("\n[3] Copying Scales (Eigenvalues)...")
    
    scales_src = VERIFICATION_OUTPUT / "scales.bin"
    
    if scales_src.exists():
        shutil.copy(scales_src, HLS_GOLDEN_DIR / "scales.bin")
        
        scales = np.fromfile(scales_src, dtype=np.float32)
        print(f"  ✓ scales.bin: {len(scales)} floats")
        print(f"  Values: {scales}")
    else:
        print("  ✗ ERROR: scales.bin not found!")
        return False
    
    # ============================================================================
    # 复制预期输出图像
    # ============================================================================
    
    print("\n[4] Copying Expected Output Image...")
    
    image_src = VERIFICATION_OUTPUT / "image.bin"
    
    if image_src.exists():
        shutil.copy(image_src, HLS_GOLDEN_DIR / "image_expected.bin")
        
        image = np.fromfile(image_src, dtype=np.float32)
        print(f"  ✓ image_expected.bin: {len(image)} floats")
        print(f"  Shape: 512x512")
        print(f"  Statistics:")
        print(f"    Mean: {image.mean():.6f}")
        print(f"    Max:  {image.max():.6f}")
        print(f"    Min:  {image.min():.6f}")
    else:
        print("  ✗ ERROR: image.bin not found!")
        return False
    
    # ============================================================================
    # 生成配置信息文件
    # ============================================================================
    
    print("\n[5] Generating HLS Configuration File...")
    
    config_file = HLS_GOLDEN_DIR / "hls_config.txt"
    with open(config_file, 'w') as f:
        f.write("# SOCS HLS Test Configuration\n")
        f.write("# Auto-generated from verification output\n")
        f.write("\n")
        f.write(f"Lx = 512\n")
        f.write(f"Ly = 512\n")
        f.write(f"Nx = 4\n")
        f.write(f"Ny = 4\n")
        f.write(f"nk = {nk}\n")
        f.write(f"krnSizeX = 9\n")
        f.write(f"krnSizeY = 9\n")
        f.write(f"sizeX = 17\n")
        f.write(f"sizeY = 17\n")
        f.write("\n")
        f.write("# Test Parameters\n")
        f.write(f"Input Files:\n")
        f.write(f"  mskf_r.bin, mskf_i.bin: 512x512 complex float32\n")
        f.write(f"  krn_*.bin: {nk} kernels, each 9x9 complex float32\n")
        f.write(f"  scales.bin: {nk} float32 eigenvalues\n")
        f.write(f"Output Files:\n")
        f.write(f"  image_expected.bin: 512x512 float32\n")
    
    print(f"  ✓ hls_config.txt created")
    
    # ============================================================================
    # 完成总结
    # ============================================================================
    
    print("\n" + "=" * 60)
    print("Golden Data Generation Completed Successfully!")
    print("=" * 60)
    print(f"Output Directory: {HLS_GOLDEN_DIR}")
    print("\nGenerated Files:")
    print("  - mskf_r.bin, mskf_i.bin (Mask frequency)")
    print("  - krn_0-9_r/i.bin (10 SOCS kernels)")
    print("  - scales.bin (Eigenvalues)")
    print("  - image_expected.bin (Expected output)")
    print("  - hls_config.txt (Test configuration)")
    print("=" * 60)
    
    return True

# ============================================================================
# 主函数
# ============================================================================

if __name__ == "__main__":
    success = copy_verification_to_hls_golden()
    
    if not success:
        print("\nERROR: Golden data generation failed!")
        print("Please run verification first:")
        print("  cd /root/project/FPGA-Litho")
        print("  python verify.py")
        exit(1)
    
    print("\nYou can now run HLS C Simulation:")
    print("  cd /root/project/FPGA-Litho/source/SOCS_HLS")
    print("  make golden  # Already done")
    print("  make csim    # Run C simulation")
    exit(0)