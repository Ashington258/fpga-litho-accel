#!/usr/bin/env python3
"""
调试脚本：对比HLS实现与CPU参考的循环边界
"""
import numpy as np
import math

# 配置参数
NA = 0.8
LAMBDA = 193.0
LX = 512
LY = 512
SRC_SIZE = 101
NX = 4
NY = 4
OUTER_SIGMA = 0.9

# 归一化参数
Lx_norm = LX * NA / LAMBDA
Ly_norm = LY * NA / LAMBDA
sh = (SRC_SIZE - 1) // 2
oSgm = int(math.ceil(sh * OUTER_SIGMA))

print("=== 归一化参数 ===")
print(f"Lx_norm = {Lx_norm:.4f}")
print(f"Ly_norm = {Ly_norm:.4f}")
print(f"sh = {sh}, oSgm = {oSgm}")

# 模拟几个光源点的循环边界
test_points = [
    (0, 0),     # 中心光源点
    (-oSgm, -oSgm),  # 左下角光源点
    (oSgm, oSgm),    # 右上角光源点
    (10, 10),    # 偏移光源点
]

print("\n=== 光源点循环边界测试 ===")
for p, q in test_points:
    sx = p / sh
    sy = q / sh
    
    # CPU参考的边界计算方式
    nxMinTmp = (-1.0 - sx) * Lx_norm
    nxMaxTmp = ( 1.0 - sx) * Lx_norm
    nyMinTmp = (-1.0 - sy) * Ly_norm
    nyMaxTmp = ( 1.0 - sy) * Ly_norm
    
    print(f"\n光源点 (p={p}, q={q}):")
    print(f"  sx={sx:.4f}, sy={sy:.4f}")
    print(f"  nxMinTmp={nxMinTmp:.4f}, nxMaxTmp={nxMaxTmp:.4f}")
    print(f"  nyMinTmp={nyMinTmp:.4f}, nyMaxTmp={nyMaxTmp:.4f}")
    print(f"  循环范围: nx=[{int(nxMinTmp)}, {int(nxMaxTmp)}], ny=[{int(nyMinTmp)}, {int(nyMaxTmp)}]")
    
    # 检查实际空间频率范围
    actual_nx = []
    actual_ny = []
    for ny in range(int(nyMinTmp), int(nyMaxTmp) + 1):
        fy = ny / Ly_norm + sy
        if fy * fy <= 1.0:
            actual_ny.append(ny)
            for nx in range(int(nxMinTmp), int(nxMaxTmp) + 1):
                fx = nx / Lx_norm + sx
                rho2 = fx * fx + fy * fy
                if rho2 <= 1.0:
                    actual_nx.append(nx)
    
    print(f"  实际有效范围: nx=[{min(actual_nx) if actual_nx else 'none'}, {max(actual_nx) if actual_nx else 'none'}]")
    print(f"  实际有效范围: ny=[{min(actual_ny) if actual_ny else 'none'}, {max(actual_ny) if actual_ny else 'none'}]")

# 计算TCC矩阵的有效索引范围
print("\n=== TCC矩阵索引范围 ===")
tcc_size = (2 * NX + 1) * (2 * NY + 1)
print(f"tcc_size = {tcc_size}")
print(f"TCC矩阵维度: {2*NX+1} x {2*NY+1} = {tcc_size}")
print(f"有效空间频率范围: nx=[-{NX}, {NX}], ny=[-{NY}, {NY}]")

# 检查中心光源点产生的pupil向量长度
sx_center = 0.0
sy_center = 0.0
nx_min_center = int((-1.0 - sx_center) * Lx_norm)
nx_max_center = int(( 1.0 - sx_center) * Lx_norm)
ny_min_center = int((-1.0 - sy_center) * Ly_norm)
ny_max_center = int(( 1.0 - sy_center) * Ly_norm)

pupil_count_center = 0
for ny in range(ny_min_center, ny_max_center + 1):
    fy = ny / Ly_norm + sy_center
    if fy * fy <= 1.0:
        for nx in range(nx_min_center, nx_max_center + 1):
            fx = nx / Lx_norm + sx_center
            rho2 = fx * fx + fy * fy
            if rho2 <= 1.0:
                pupil_count_center += 1

print(f"\n中心光源点产生的pupil元素数量: {pupil_count_center}")
print(f"理论上应该覆盖整个TCC空间频率范围: {tcc_size}")

# 检查是否超出TCC边界
print("\n=== 边界检查 ===")
print(f"nxMin_center={nx_min_center}, nxMax_center={nx_max_center}")
print(f"nyMin_center={ny_min_center}, nyMax_center={ny_max_center}")
print(f"TCC边界: nx=[-{NX}, {NX}], ny=[-{NY}, {NY}]")
if nx_min_center < -NX or nx_max_center > NX:
    print("⚠ Warning: nx range exceeds TCC bounds!")
if ny_min_center < -NY or ny_max_center > NY:
    print("⚠ Warning: ny range exceeds TCC bounds!")