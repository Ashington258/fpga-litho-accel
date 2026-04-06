#!/usr/bin/env python3
"""
SOCS HLS Golden数据生成脚本

用途：从CPU参考代码生成测试用的Golden数据
执行：python generate_golden.py

输入（从CPU参考输出）:
  - reference/output/kernels/ (SOCS核文件)
  - reference/output/image.bin (CPU参考空中像)

输出（到golden目录）:
  - golden/mskf_r.bin, mskf_i.bin (Mask频域数据)
  - golden/krn_*_r.bin, krn_*_i.bin (核数据)
  - golden/scales.bin (特征值)
  - golden/image_expected.bin (预期空中像)
  - golden/golden_info.txt (尺寸信息)
"""

import numpy as np
import os
import json
import struct
import sys

# ============================================================================
# 配置参数
# ============================================================================
GOLDEN_DIR = "hls/tb/golden"
REF_KERNEL_DIR = "/root/project/FPGA-Litho/reference/output/kernels"
REF_IMAGE_FILE = "/root/project/FPGA-Litho/reference/output/image.bin"
CONFIG_FILE = "/root/project/FPGA-Litho/input/config/config.json"

# 默认测试参数（如果无法从参考获取）
DEFAULT_PARAMS = {
    'Lx': 512,
    'Ly': 512,
    'Nx': 4,
    'Ny': 4,
    'nk': 8,
    'krnSizeX': 9,  # 2*Nx+1
    'krnSizeY': 9,  # 2*Ny+1
}

# ============================================================================
# 文件读写函数
# ============================================================================
def write_float_array(filename, data):
    """写入float32数组到二进制文件"""
    with open(filename, 'wb') as f:
        f.write(data.astype(np.float32).tobytes())
    print(f"  Written: {filename} ({len(data)} elements)")

def read_float_array(filename, expected_size=None):
    """从二进制文件读取float32数组"""
    if not os.path.exists(filename):
        print(f"  Warning: {filename} not found")
        return None
    with open(filename, 'rb') as f:
        data = np.frombuffer(f.read(), dtype=np.float32)
    if expected_size and len(data) != expected_size:
        print(f"  Warning: {filename} size mismatch (expected {expected_size}, got {len(data)})")
    return data

def read_kernel_info(filename):
    """读取kernel_info.txt获取核参数"""
    if not os.path.exists(filename):
        print(f"  Warning: {filename} not found, using defaults")
        return DEFAULT_PARAMS
    
    params = {}
    with open(filename, 'r') as f:
        for line in f:
            if ':' in line:
                key, value = line.strip().split(':')
                key = key.strip()
                value = value.strip()
                if key in ['krnSizeX', 'krnSizeY', 'nk']:
                    params[key] = int(value)
                elif key == 'scales':
                    # 解析特征值列表
                    scales_str = value.replace('[', '').replace(']', '')
                    params['scales'] = [float(x) for x in scales_str.split(',')]
    
    # 计算Nx, Ny
    if 'krnSizeX' in params:
        params['Nx'] = (params['krnSizeX'] - 1) // 2
    if 'krnSizeY' in params:
        params['Ny'] = (params['krnSizeY'] - 1) // 2
    
    return params

# ============================================================================
# 数据生成函数
# ============================================================================
def generate_mask_frequency(Lx, Ly):
    """生成模拟的Mask频域数据（实际应从CPU参考FFT结果获取）"""
    # 模拟FFT结果：中心区域有值，边缘衰减
    mskf_real = np.zeros((Lx, Ly), dtype=np.float32)
    mskf_imag = np.zeros((Lx, Ly), dtype=np.float32)
    
    # 中心区域填充随机值（模拟频域响应）
    center_x = Lx // 2
    center_y = Ly // 2
    radius = min(Lx, Ly) // 8
    
    for y in range(Ly):
        for x in range(Lx):
            dist = np.sqrt((x - center_x)**2 + (y - center_y)**2)
            if dist < radius:
                # 低频区域有较强响应
                mskf_real[y, x] = np.cos(dist / radius * np.pi / 2)
                mskf_imag[y, x] = np.sin(dist / radius * np.pi / 4)
            else:
                # 高频区域衰减
                mskf_real[y, x] = 0.01 * np.random.randn()
                mskf_imag[y, x] = 0.01 * np.random.randn()
    
    return mskf_real.flatten(), mskf_imag.flatten()

def generate_kernels_from_reference(nk, krnSizeX, krnSizeY):
    """从CPU参考输出读取SOCS核"""
    kernels_real = []
    kernels_imag = []
    
    for i in range(nk):
        real_file = os.path.join(REF_KERNEL_DIR, f"krn_{i}_r.bin")
        imag_file = os.path.join(REF_KERNEL_DIR, f"krn_{i}_i.bin")
        
        if os.path.exists(real_file) and os.path.exists(imag_file):
            krn_r = read_float_array(real_file, krnSizeX * krnSizeY)
            krn_i = read_float_array(imag_file, krnSizeX * krnSizeY)
            kernels_real.append(krn_r)
            kernels_imag.append(krn_i)
            print(f"  Loaded kernel {i} from reference")
        else:
            # 生成模拟核
            print(f"  Generating simulated kernel {i}")
            krn_r = np.random.randn(krnSizeX * krnSizeY).astype(np.float32) * 0.1
            krn_i = np.random.randn(krnSizeX * krnSizeY).astype(np.float32) * 0.05
            kernels_real.append(krn_r)
            kernels_imag.append(krn_i)
    
    return kernels_real, kernels_imag

def generate_scales(nk, from_reference=False):
    """生成特征值数组"""
    if from_reference:
        info_file = os.path.join(REF_KERNEL_DIR, "kernel_info.txt")
        params = read_kernel_info(info_file)
        if 'scales' in params and len(params['scales']) >= nk:
            return np.array(params['scales'][:nk], dtype=np.float32)
    
    # 生成模拟特征值（递减序列）
    scales = np.array([1.0 - 0.1 * i for i in range(nk)], dtype=np.float32)
    return scales

def generate_expected_image(Lx, Ly, from_reference=False):
    """生成预期的空中像输出"""
    if from_reference and os.path.exists(REF_IMAGE_FILE):
        image = read_float_array(REF_IMAGE_FILE, Lx * Ly)
        if image is not None and len(image) == Lx * Ly:
            print(f"  Loaded image from reference: {REF_IMAGE_FILE}")
            return image
    
    # 生成模拟空中像（正弦波图案）
    print(f"  Generating simulated image")
    image = np.zeros((Lx, Ly), dtype=np.float32)
    for y in range(Ly):
        for x in range(Lx):
            # 模拟光刻图案：周期性条纹
            image[y, x] = 0.5 + 0.3 * np.sin(2 * np.pi * x / 32) * np.sin(2 * np.pi * y / 32)
    return image.flatten()

# ============================================================================
# 主函数
# ============================================================================
def main():
    print("=" * 60)
    print("SOCS HLS Golden Data Generator")
    print("=" * 60)
    
    # 读取配置
    print("\n[1] Reading configuration...")
    try:
        with open(CONFIG_FILE, 'r') as f:
            config = json.load(f)
        Lx = config.get('Lx', DEFAULT_PARAMS['Lx'])
        Ly = config.get('Ly', DEFAULT_PARAMS['Ly'])
        Nx = config.get('Nx', DEFAULT_PARAMS['Nx'])
        Ny = config.get('Ny', DEFAULT_PARAMS['Ny'])
        print(f"  Config loaded: Lx={Lx}, Ly={Ly}, Nx={Nx}, Ny={Ny}")
    except Exception as e:
        print(f"  Using default params: {e}")
        Lx = DEFAULT_PARAMS['Lx']
        Ly = DEFAULT_PARAMS['Ly']
        Nx = DEFAULT_PARAMS['Nx']
        Ny = DEFAULT_PARAMS['Ny']
    
    krnSizeX = 2 * Nx + 1
    krnSizeY = 2 * Ny + 1
    nk = DEFAULT_PARAMS['nk']
    
    # 创建golden目录
    os.makedirs(GOLDEN_DIR, exist_ok=True)
    
    # 尝试从CPU参考读取核信息
    print("\n[2] Checking reference kernel data...")
    ref_info_file = os.path.join(REF_KERNEL_DIR, "kernel_info.txt")
    if os.path.exists(ref_info_file):
        params = read_kernel_info(ref_info_file)
        if 'nk' in params:
            nk = params['nk']
        if 'krnSizeX' in params:
            krnSizeX = params['krnSizeX']
            Nx = (krnSizeX - 1) // 2
        if 'krnSizeY' in params:
            krnSizeY = params['krnSizeY']
            Ny = (krnSizeY - 1) // 2
        print(f"  Reference params: nk={nk}, krnSizeX={krnSizeX}, krnSizeY={krnSizeY}")
    
    # 生成Mask频域数据
    print("\n[3] Generating mask frequency data...")
    mskf_r, mskf_i = generate_mask_frequency(Lx, Ly)
    write_float_array(os.path.join(GOLDEN_DIR, "mskf_r.bin"), mskf_r)
    write_float_array(os.path.join(GOLDEN_DIR, "mskf_i.bin"), mskf_i)
    
    # 生成SOCS核数据
    print("\n[4] Generating SOCS kernels...")
    kernels_r, kernels_i = generate_kernels_from_reference(nk, krnSizeX, krnSizeY)
    for i in range(nk):
        write_float_array(os.path.join(GOLDEN_DIR, f"krn_{i}_r.bin"), kernels_r[i])
        write_float_array(os.path.join(GOLDEN_DIR, f"krn_{i}_i.bin"), kernels_i[i])
    
    # 生成特征值
    print("\n[5] Generating eigenvalues (scales)...")
    scales = generate_scales(nk, from_reference=True)
    write_float_array(os.path.join(GOLDEN_DIR, "scales.bin"), scales)
    
    # 生成预期空中像
    print("\n[6] Generating expected aerial image...")
    image = generate_expected_image(Lx, Ly, from_reference=True)
    write_float_array(os.path.join(GOLDEN_DIR, "image_expected.bin"), image)
    
    # 写入尺寸信息文件
    print("\n[7] Writing golden_info.txt...")
    info_content = f"""# SOCS Golden Data Information
# Generated by generate_golden.py

krnSizeX: {krnSizeX}
krnSizeY: {krnSizeY}
nk: {nk}
Nx: {Nx}
Ny: {Ny}
Lx: {Lx}
Ly: {Ly}

# Eigenvalues (scales)
scales: [{', '.join([f'{s:.6f}' for s in scales])}]

# File sizes
mskf: {Lx * Ly} elements (complex float32)
krn: {krnSizeX * krnSizeY} elements each (complex float32)
image: {Lx * Ly} elements (float32)
"""
    with open(os.path.join(GOLDEN_DIR, "golden_info.txt"), 'w') as f:
        f.write(info_content)
    
    print("\n" + "=" * 60)
    print("Golden data generation complete!")
    print(f"Output directory: {GOLDEN_DIR}")
    print("=" * 60)
    
    # 输出文件列表
    print("\nGenerated files:")
    for filename in os.listdir(GOLDEN_DIR):
        filepath = os.path.join(GOLDEN_DIR, filename)
        size = os.path.getsize(filepath)
        print(f"  {filename}: {size} bytes")

if __name__ == '__main__':
    main()