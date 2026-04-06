#!/usr/bin/env python3
"""
调试循环边界差异：对比floor vs int转换
"""
import numpy as np
import math

# 配置
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
sh = (SRC_SIZE - 1) // 2
oSgm = int(math.ceil(sh * OUTER_SIGMA))

print("=== 循环边界计算方式对比 ===")
print(f"Lx_norm={Lx_norm:.4f}, Ly_norm={Ly_norm:.4f}")

# 测试几个光源点
test_cases = [
    (0, 0),      # 中心
    (-oSgm, -oSgm),  # 角落
    (oSgm, oSgm),
    (-20, -20),
    (20, 20),
]

for p, q in test_cases:
    sx = p / sh
    sy = q / sh
    
    # CPU参考方式（隐式floor）
    nxMinTmp_cpu = int((-1.0 - sx) * Lx_norm)
    nxMaxTmp_cpu = int(( 1.0 - sx) * Lx_norm)
    nyMinTmp_cpu = int((-1.0 - sy) * Ly_norm)
    nyMaxTmp_cpu = int(( 1.0 - sy) * Ly_norm)
    
    # Python math.floor方式
    nxMinTmp_floor = math.floor((-1.0 - sx) * Lx_norm)
    nxMaxTmp_floor = math.floor(( 1.0 - sx) * Lx_norm)
    nyMinTmp_floor = math.floor((-1.0 - sy) * Ly_norm)
    nyMaxTmp_floor = math.floor(( 1.0 - sy) * Ly_norm)
    
    print(f"\n光源点 (p={p}, q={q}), sx={sx:.4f}, sy={sy:.4f}:")
    print(f"  CPU (int):  nx=[{nxMinTmp_cpu}, {nxMaxTmp_cpu}], ny=[{nyMinTmp_cpu}, {nyMaxTmp_cpu}]")
    print(f"  Floor:      nx=[{nxMinTmp_floor}, {nxMaxTmp_floor}], ny=[{nyMinTmp_floor}, {nyMaxTmp_floor}]")
    
    if nxMinTmp_cpu != nxMinTmp_floor or nxMaxTmp_cpu != nxMaxTmp_floor:
        print("  ⚠ Difference detected!")

# 检查C++中的实际实现
print("\n=== C++边界计算验证 ===")
# 读取CPU参考代码中的实际公式
print("CPU参考代码 (klitho_tcc.cpp:218-221):")
print("  nxMinTmp = (-1 - sx) * Lx_norm;  // 自动floor")
print("  nxMaxTmp = (1 - sx) * Lx_norm;")
print("\n我的C++实现:")
print("  nxMinTmp = (-1.0 - sx) * Lx_norm;  // 乘积后转int")
print("  nxMaxTmp = (1.0 - sx) * Lx_norm;")