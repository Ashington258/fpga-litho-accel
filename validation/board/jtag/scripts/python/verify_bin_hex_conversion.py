#!/usr/bin/env python3
"""
验证环节1：BIN→HEX转换正确性验证

用途：
  - 验证bin_to_hex_for_ddr.py的转换无损
  - 通过逆向转换对比（BIN→HEX→BIN）验证格式正确性

验证方法：
  1. 读取原始BIN文件
  2. 使用bin_to_hex_for_ddr.py转换为HEX格式
  3. 使用hex_to_bin.py逆向转换回BIN
  4. 对比原始BIN与逆向BIN的数值一致性

验收标准：
  - RMSE = 0.0 (完全一致)
  - 最大绝对误差 < 1e-10

使用方法：
  python verify_bin_hex_conversion.py --input <bin_file>

示例：
  python verify_bin_hex_conversion.py --input mskf_r.bin
"""

import argparse
import struct
import numpy as np
import sys
import os

# 添加父目录到Python路径，导入bin_to_hex_for_ddr
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from bin_to_hex_for_ddr import float_to_hex, read_bin_file

# 导入hex_to_bin（逆向转换）
try:
    from hex_to_bin import hex_to_float
except ImportError:
    print("警告: hex_to_bin.py未找到，将使用内置转换函数")

    def hex_to_float(hex_str):
        """IEEE 754 hex → float32 (little-endian)"""
        hex_val = int(hex_str, 16)
        packed = struct.pack(">I", hex_val)  # big-endian unsigned int
        val = struct.unpack(">f", packed)[0]  # big-endian float
        # 转换为little-endian（与原始BIN一致）
        return val


def verify_bin_hex_conversion(bin_path):
    """验证BIN→HEX转换正确性"""

    # 步骤1：读取原始BIN
    print(f"读取原始BIN文件: {bin_path}")
    original_floats = read_bin_file(bin_path)
    original_array = np.array(original_floats, dtype=np.float32)

    print(f"  数据量: {len(original_floats)} floats")
    print(f"  原始数据范围: [{original_array.min():.6f}, {original_array.max():.6f}]")
    print(f"  原始数据均值: {original_array.mean():.6f}")

    # 步骤2：转换为HEX格式
    print(f"\n转换为HEX格式...")
    hex_values = [float_to_hex(v) for v in original_floats]

    # 检查HEX格式正确性
    print(f"  HEX样本（前5个）:")
    for i in range(min(5, len(hex_values))):
        print(f"    {original_floats[i]:.6f} → {hex_values[i]}")

    # 步骤3：逆向转换回BIN
    print(f"\n逆向转换回BIN...")
    reconstructed_floats = [hex_to_float(hex_val) for hex_val in hex_values]
    reconstructed_array = np.array(reconstructed_floats, dtype=np.float32)

    # 步骤4：对比一致性
    print(f"\n对比一致性...")

    # 计算误差
    errors = np.abs(original_array - reconstructed_array)
    rmse = np.sqrt(np.mean(errors**2))
    max_error = np.max(errors)
    mean_error = np.mean(errors)

    # 验证结果
    print(f"\n验证结果:")
    print(f"  RMSE: {rmse:.10e}")
    print(f"  最大绝对误差: {max_error:.10e}")
    print(f"  平均绝对误差: {mean_error:.10e}")

    # 验收标准
    PASS_THRESHOLD = 1e-10

    if rmse < PASS_THRESHOLD and max_error < PASS_THRESHOLD:
        print(f"\n✅ 验证通过：BIN→HEX转换无损 (RMSE={rmse:.10e} < {PASS_THRESHOLD})")
        return True
    else:
        print(
            f"\n❌ 验证失败：转换存在误差 (RMSE={rmse:.10e}, max_error={max_error:.10e})"
        )

        # 打印差异较大的样本
        print(f"\n差异样本（误差 > 1e-12）：")
        large_errors_idx = np.where(errors > 1e-12)[0]
        for idx in large_errors_idx[:10]:  # 仅显示前10个
            print(
                f"  idx={idx}: 原始={original_floats[idx]:.6f}, 重构={reconstructed_floats[idx]:.6f}, 误差={errors[idx]:.10e}"
            )

        return False


def main():
    parser = argparse.ArgumentParser(description="验证BIN→HEX转换正确性")
    parser.add_argument("--input", required=True, help="输入BIN文件路径")
    args = parser.parse_args()

    # 验证文件存在
    if not os.path.exists(args.input):
        print(f"错误: 文件不存在 {args.input}")
        return

    # 执行验证
    success = verify_bin_hex_conversion(args.input)

    if success:
        print(f"\n验证完成：BIN→HEX转换信任链环节1验证通过 ✅")
    else:
        print(f"\n验证完成：BIN→HEX转换信任链环节1验证失败 ❌")


if __name__ == "__main__":
    main()
