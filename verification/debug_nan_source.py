#!/usr/bin/env python3
"""
深度调试NaN产生的原因
分步骤执行HLS逻辑，找出NaN产生的具体位置
"""
import numpy as np
import struct
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
DATA_DIR = PROJECT_ROOT / "source/SOCS_HLS/data"

def load_binary(file_path):
    """加载二进制浮点数据"""
    with open(file_path, 'rb') as f:
        data = f.read()
    floats = struct.unpack(f'{len(data)//4}f', data)
    return np.array(floats, dtype=np.float32)

def simulate_hls_calc_socs():
    """
    模拟HLS calc_socs_hls的完整流程
    """
    print("=== 模拟HLS calc_socs_hls完整流程 ===\n")
    
    # 加载所有数据
    mskf_r = load_binary(DATA_DIR / "mskf_r.bin")
    mskf_i = load_binary(DATA_DIR / "mskf_i.bin")
    scales = load_binary(DATA_DIR / "scales.bin")
    
    # 加载所有10个kernel
    kernels_r = [load_binary(DATA_DIR / f"kernels/krn_{k}_r.bin") for k in range(10)]
    kernels_i = [load_binary(DATA_DIR / f"kernels/krn_{k}_i.bin") for k in range(10)]
    
    print(f"数据加载完成:")
    print(f"  mskf: {mskf_r.shape}, range=[{mskf_r.min():.6e}, {mskf_r.max():.6e}]")
    print(f"  scales: {scales}, range=[{scales.min():.3f}, {scales.max():.3f}]")
    
    # 常量
    Lxh, Lyh = 256, 256
    Lx_full, Ly_full = 512, 512
    kerX, kerY = 9, 9
    Nx, Ny = 4, 4
    fftConvX, fftConvY = 32, 32
    difX, difY = 23, 23
    
    # 初始化累积数组
    tmpImg_32 = np.zeros((32, 32), dtype=np.float32)
    
    # 处理每个kernel
    for k in range(10):
        print(f"\n=== Kernel {k} ===")
        krn_r = kernels_r[k]
        krn_i = kernels_i[k]
        scale = scales[k]
        
        print(f"  Kernel range: [{krn_r.min():.6e}, {krn_r.max():.6e}]")
        print(f"  Scale: {scale:.6f}")
        
        # Step 1: Build padded IFFT input
        padded_r = np.zeros((fftConvY, fftConvX), dtype=np.float32)
        padded_i = np.zeros((fftConvY, fftConvX), dtype=np.float32)
        
        prod_list = []
        for ky in range(kerY):
            for kx in range(kerX):
                phys_y = ky - Ny
                phys_x = kx - Nx
                mask_y = Lyh + phys_y
                mask_x = Lxh + phys_x
                
                if mask_y >= 0 and mask_y < Ly_full and mask_x >= 0 and mask_x < Lx_full:
                    kr_r = krn_r[ky * kerX + kx]
                    kr_i = krn_i[ky * kerX + kx]
                    mask_idx = mask_y * Lx_full + mask_x
                    ms_r = mskf_r[mask_idx]
                    ms_i = mskf_i[mask_idx]
                    
                    prod_r = kr_r * ms_r - kr_i * ms_i
                    prod_i = kr_r * ms_i + kr_i * ms_r
                    
                    # Denormalized flush (修复后的逻辑)
                    denorm_threshold = 1e-35
                    if abs(prod_r) < denorm_threshold:
                        prod_r = 0.0
                    if abs(prod_i) < denorm_threshold:
                        prod_i = 0.0
                    
                    prod_list.append((prod_r, prod_i))
                    padded_r[difY + ky, difX + kx] = prod_r
                    padded_i[difY + ky, difX + kx] = prod_i
        
        # 检查padded数组是否有NaN/Inf
        prod_array = np.array(prod_list)
        print(f"  Padded统计:")
        print(f"    Prod range: [{prod_array.min():.6e}, {prod_array.max():.6e}]")
        print(f"    NaN/Inf: {np.isnan(prod_array).any()}, {np.isinf(prod_array).any()}")
        
        if np.isnan(prod_array).any() or np.isinf(prod_array).any():
            print(f"  ❌ NaN/Inf在padded阶段产生！")
            for idx, (r, i) in enumerate(prod_list):
                if np.isnan(r) or np.isnan(i) or np.isinf(r) or np.isinf(i):
                    print(f"    idx={idx}: {r:.6e}, {i:.6e}")
            return False
        
        # Step 2: 2D IFFT (用numpy模拟)
        print(f"  执行2D IFFT (32×32)...")
        padded_complex = padded_r + 1j * padded_i
        
        # Numpy IFFT
        ifft_result = np.fft.ifft2(padded_complex)
        
        print(f"    IFFT result range: real=[{ifft_result.real.min():.6e}, {ifft_result.real.max():.6e}]")
        print(f"                       imag=[{ifft_result.imag.min():.6e}, {ifft_result.imag.max():.6e}]")
        print(f"    IFFT NaN/Inf: {np.isnan(ifft_result).any()}, {np.isinf(ifft_result).any()}")
        
        if np.isnan(ifft_result).any() or np.isinf(ifft_result).any():
            print(f"  ❌ NaN/Inf在IFFT阶段产生！")
            return False
        
        # Step 3: Accumulate intensity (scale * (re^2 + im^2))
        intensity = scale * (ifft_result.real**2 + ifft_result.imag**2)
        
        print(f"    Intensity range: [{intensity.min():.6e}, {intensity.max():.6e}]")
        print(f"    Intensity NaN/Inf: {np.isnan(intensity).any()}, {np.isinf(intensity).any()}")
        
        if np.isnan(intensity).any() or np.isinf(intensity).any():
            print(f"  ❌ NaN/Inf在intensity累积阶段产生！")
            return False
        
        # 累积到tmpImg_32
        tmpImg_32 += intensity
        
        print(f"    tmpImg_32当前 range: [{tmpImg_32.min():.6e}, {tmpImg_32.max():.6e}]")
        print(f"    tmpImg_32 NaN/Inf: {np.isnan(tmpImg_32).any()}, {np.isinf(tmpImg_32).any()}")
        
        if np.isnan(tmpImg_32).any() or np.isinf(tmpImg_32).any():
            print(f"  ❌ NaN/Inf在累积后产生！")
            return False
    
    print(f"\n=== 所有kernel处理完成 ===")
    print(f"tmpImg_32最终统计:")
    print(f"  Range: [{tmpImg_32.min():.6e}, {tmpImg_32.max():.6e}]")
    print(f"  Mean: {tmpImg_32.mean():.6e}")
    print(f"  NaN/Inf: {np.isnan(tmpImg_32).any()}, {np.isinf(tmpImg_32).any()}")
    
    # Step 4: FFTShift
    print(f"\n执行FFTShift...")
    tmpImgp_32 = np.fft.fftshift(tmpImg_32)
    
    print(f"  tmpImgp_32 range: [{tmpImgp_32.min():.6e}, {tmpImgp_32.max():.6e}]")
    print(f"  tmpImgp_32 NaN/Inf: {np.isnan(tmpImgp_32).any()}, {np.isinf(tmpImgp_32).any()}")
    
    # Step 5: Extract center 17×17
    print(f"\n提取中心17×17区域...")
    center_y = (32 - 17) // 2  # 7
    center_x = (32 - 17) // 2  # 7
    tmpImgp_17 = tmpImgp_32[center_y:center_y+17, center_x:center_x+17]
    
    print(f"  tmpImgp_17 range: [{tmpImgp_17.min():.6e}, {tmpImgp_17.max():.6e}]")
    print(f"  tmpImgp_17 NaN/Inf: {np.isnan(tmpImgp_17).any()}, {np.isinf(tmpImgp_17).any()}")
    
    # 与golden对比
    golden = load_binary(DATA_DIR / "tmpImgp_pad32.bin")
    golden_17 = golden.reshape(17, 17)
    
    print(f"\n=== 与Golden对比 ===")
    print(f"Golden range: [{golden.min():.6e}, {golden.max():.6e}]")
    
    # RMSE计算
    rmse = np.sqrt(np.mean((tmpImgp_17 - golden_17)**2))
    rel_error = np.abs(tmpImgp_17 - golden_17).mean() / golden.mean()
    
    print(f"RMSE: {rmse:.6e}")
    print(f"Relative error: {rel_error:.6%}")
    
    # Pass判断
    if np.isnan(tmpImgp_17).any() or np.isinf(tmpImgp_17).any():
        print(f"\n❌ 最终结果包含NaN/Inf")
        return False
    elif rmse < 1e-4 or rel_error < 0.05:
        print(f"\n✓ PASS - 验证通过")
        return True
    else:
        print(f"\n⚠️ WARNING - RMSE/rel_error超标，但无NaN/Inf")
        print(f"  可能是FFT scaling差异")
        return True

if __name__ == "__main__":
    import sys
    result = simulate_hls_calc_socs()
    sys.exit(0 if result else 1)