#!/usr/bin/env python3
"""
算法验证脚本：逐步对比HLS与CPU参考的实现差异
"""
import numpy as np
import struct
import math

# 配置参数
NA = 0.8
LAMBDA = 193.0
LX = 512
LY = 512
SRC_SIZE = 101
NX = 4
NY = 4
OUTER_SIGMA = 0.6

# 归一化参数
Lx_norm = LX * NA / LAMBDA
Ly_norm = LY * NA / LAMBDA
dz = 0.2 / (NA * NA / LAMBDA)
k = 2 * math.pi / LAMBDA
sh = (SRC_SIZE - 1) // 2
oSgm = int(math.ceil(sh * OUTER_SIGMA))
tcc_size = (2 * NX + 1) * (2 * NY + 1)

print("=== 参数配置 ===")
print(f"NA={NA}, lambda={LAMBDA}nm")
print(f"Lx={LX}, Ly={LY}")
print(f"Nx={NX}, Ny={NY}")
print(f"Lx_norm={Lx_norm:.4f}, Ly_norm={Ly_norm:.4f}")
print(f"sh={sh}, oSgm={oSgm}")
print(f"tcc_size={tcc_size}")

# 加载光源数据
src = np.fromfile('golden/source_101x101.bin', dtype=np.float32)
print(f"\n光源数据: {np.sum(src != 0)} 个非零元素")

# 模拟CPU参考的pupil矩阵计算
pupil = np.zeros((tcc_size, SRC_SIZE * SRC_SIZE), dtype=complex)
tcc = np.zeros((tcc_size, tcc_size), dtype=complex)

print("\n=== 第一次循环：计算pupil矩阵 ===")
pupil_count = 0
for q in range(-oSgm, oSgm + 1):
    for p in range(-oSgm, oSgm + 1):
        srcID = (q + sh) * SRC_SIZE + (p + sh)
        
        if src[srcID] != 0 and (p * p + q * q) <= sh * sh:
            sx = p / sh
            sy = q / sh
            
            nxMinTmp = int((-1.0 - sx) * Lx_norm)
            nxMaxTmp = int(( 1.0 - sx) * Lx_norm)
            nyMinTmp = int((-1.0 - sy) * Ly_norm)
            nyMaxTmp = int(( 1.0 - sy) * Ly_norm)
            
            for ny in range(nyMinTmp, nyMaxTmp + 1):
                fy = ny / Ly_norm + sy
                if fy * fy <= 1.0:
                    for nx in range(nxMinTmp, nxMaxTmp + 1):
                        fx = nx / Lx_norm + sx
                        rho2 = fx * fx + fy * fy
                        if rho2 <= 1.0:
                            phase = dz * k * math.sqrt(1.0 - rho2 * NA * NA)
                            pupil_val = complex(math.cos(phase), math.sin(phase))
                            
                            ID1 = (ny + NY) * (2 * NX + 1) + (nx + NX)
                            pupil[ID1, srcID] = pupil_val
                            pupil_count += 1

print(f"Pupil矩阵非零元素: {pupil_count}")

# 检查pupil矩阵结构
nonzero_per_row = np.sum(pupil != 0, axis=1)
print(f"Pupil矩阵每行非零元素数（前10行）: {nonzero_per_row[:10]}")
print(f"Pupil矩阵非零行数: {np.sum(nonzero_per_row > 0)}")

print("\n=== 第二次循环：累积TCC ===")
accum_count = 0
for q in range(-oSgm, oSgm + 1):
    for p in range(-oSgm, oSgm + 1):
        srcID = (q + sh) * SRC_SIZE + (p + sh)
        
        if src[srcID] != 0 and (p * p + q * q) <= sh * sh:
            srcVal = complex(src[srcID], 0)
            sx = p / sh
            sy = q / sh
            
            nxMinTmp = int((-1.0 - sx) * Lx_norm)
            nxMaxTmp = int(( 1.0 - sx) * Lx_norm)
            nyMinTmp = int((-1.0 - sy) * Ly_norm)
            nyMaxTmp = int(( 1.0 - sy) * Ly_norm)
            
            for ny in range(nyMinTmp, nyMaxTmp + 1):
                for nx in range(nxMinTmp, nxMaxTmp + 1):
                    ID1 = (ny + NY) * (2 * NX + 1) + (nx + NX)
                    
                    if pupil[ID1, srcID] != complex(0, 0):
                        tmpValComplex = pupil[ID1, srcID] * srcVal
                        
                        for my in range(ny, nyMaxTmp + 1):
                            startmx = nx if ny == my else nxMinTmp
                            
                            for mx in range(startmx, nxMaxTmp + 1):
                                ID2 = (my + NY) * (2 * NX + 1) + (mx + NX)
                                tcc[ID1, ID2] += tmpValComplex * np.conj(pupil[ID2, srcID])
                                accum_count += 1

print(f"TCC累积次数: {accum_count}")

# 填充下三角
for i in range(tcc_size):
    for j in range(i + 1, tcc_size):
        tcc[j, i] = np.conj(tcc[i, j])

# 统计
tcc_real = tcc.real.flatten()
print(f"\nPython实现统计:")
print(f"  Mean: {np.mean(tcc_real):.6f}")
print(f"  StdDev: {np.std(tcc_real):.6f}")
print(f"  Non-zero elements: {np.sum(tcc_real != 0)}")

# 对比golden
golden_r = np.fromfile('golden/tcc_expected_r.bin', dtype=np.float32)
golden_real = golden_r.reshape(tcc_size, tcc_size)

print(f"\nGolden统计:")
print(f"  Mean: {np.mean(golden_r):.6f}")
print(f"  StdDev: {np.std(golden_r):.6f}")
print(f"  Non-zero elements: {np.sum(golden_r != 0)}")

# 中心元素对比
center_idx = tcc_size // 2
print(f"\n中心元素 TCC[{center_idx},{center_idx}]:")
print(f"  Python: {tcc[center_idx, center_idx]}")
print(f"  Golden: {golden_real[center_idx, center_idx]:.6f}")

# 误差分析
diff = np.abs(tcc_real - golden_r)
print(f"\n误差分析:")
print(f"  Max absolute error: {np.max(diff):.6f}")
print(f"  Mean absolute error: {np.mean(diff):.6f}")