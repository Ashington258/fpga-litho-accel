#!/usr/bin/env python3
"""
验证环节3：HLS计算输出正确性验证

用途：
  - 验证HLS IP正确解析DDR数据并输出正确结果
  - 对比HLS输出与Golden参考的一致性

验证方法：
  1. 从DDR读取HLS输出数据（aerial image）
  2. 将DDR回读数据转换为float32数组
  3. 对比HLS输出与Golden参考（aerial_image_tcc_direct.bin）

验收标准：
  - RMSE < 1e-6 (与Golden一致)
  - 最大相对误差 < 1%

使用方法：
  python verify_hls_output.py --golden <golden_bin> --hls_ddr_addr <hex_addr> --hls_output_len <int>

示例：
  python verify_hls_output.py --golden aerial_image_tcc_direct.bin --hls_ddr_addr 0x44840000 --hls_output_len 289
"""

import argparse
import struct
import numpy as np
import os

# DDR回读数据路径（由TCL脚本生成）
DDR_READ_FILE = "ddr_read_output.txt"


def hex_to_float_array(hex_file_path):
    """从DDR回读HEX文件转换为float32数组"""

    floats = []
    with open(hex_file_path, "r") as f:
        # 读取HEX数据（格式：XXXXXXXX_XXXXXXXX_...）
        line = f.read().strip()

        # 分割HEX值
        hex_values = line.split("_")

        for hex_val in hex_values:
            # IEEE 754 hex → float32
            hex_int = int(hex_val, 16)
            packed = struct.pack(">I", hex_int)  # big-endian unsigned int
            val = struct.unpack(">f", packed)[0]  # big-endian float
            floats.append(val)

    return np.array(floats, dtype=np.float32)


def read_golden_bin(bin_path):
    """读取Golden参考BIN文件"""

    floats = []
    with open(bin_path, "rb") as f:
        while True:
            data = f.read(4)
            if not data:
                break
            # little-endian float32
            val = struct.unpack("<f", data)[0]
            floats.append(val)

    return np.array(floats, dtype=np.float32)


def verify_hls_output(golden_path, hls_output_len):
    """验证HLS输出正确性"""

    # 步骤1：检查DDR回读文件是否存在
    if not os.path.exists(DDR_READ_FILE):
        print(f"错误: DDR回读文件不存在 {DDR_READ_FILE}")
        print(f"请先在Vivado Tcl Console中运行DDR回读脚本，生成 {DDR_READ_FILE}")
        return False

    # 步骤2：读取Golden参考
    print(f"读取Golden参考: {golden_path}")
    golden_array = read_golden_bin(golden_path)
    print(f"  Golden数据量: {len(golden_array)} floats")
    print(f"  Golden范围: [{golden_array.min():.6f}, {golden_array.max():.6f}]")
    print(f"  Golden均值: {golden_array.mean():.6f}")

    # 步骤3：读取HLS输出（DDR回读）
    print(f"\n读取HLS输出（DDR回读）: {DDR_READ_FILE}")
    hls_array = hex_to_float_array(DDR_READ_FILE)
    print(f"  HLS数据量: {len(hls_array)} floats")
    print(f"  HLS范围: [{hls_array.min():.6f}, {hls_array.max():.6f}]")
    print(f"  HLS均值: {hls_array.mean():.6f}")

    # 步骤4：截取Golden对应尺寸
    # 注意：Golden是512×512，HLS输出是17×17或Nx相关尺寸
    golden_size = len(golden_array)
    hls_size = len(hls_array)

    if golden_size != hls_size:
        print(f"⚠️  数据尺寸不一致: Golden={golden_size}, HLS={hls_size}")
        print(f"  将截取Golden前{hls_size}个数据进行对比")
        golden_compare = golden_array[:hls_size]
    else:
        golden_compare = golden_array

    # 步骤5：对比一致性
    print(f"\n对比HLS输出与Golden参考...")

    # 计算误差
    errors = np.abs(golden_compare - hls_array)
    rmse = np.sqrt(np.mean(errors**2))
    max_error = np.max(errors)
    mean_error = np.mean(errors)

    # 计算相对误差
    golden_norm = np.abs(golden_compare)
    relative_errors = errors / (golden_norm + 1e-10)  # 避免除零
    max_relative_error = np.max(relative_errors)
    mean_relative_error = np.mean(relative_errors)

    # 验证结果
    print(f"\n验证结果:")
    print(f"  RMSE: {rmse:.10e}")
    print(f"  最大绝对误差: {max_error:.10e}")
    print(f"  平均绝对误差: {mean_error:.10e}")
    print(f"  最大相对误差: {max_relative_error:.6f} ({max_relative_error*100:.2f}%)")
    print(f"  平均相对误差: {mean_relative_error:.6f} ({mean_relative_error*100:.2f}%)")

    # 验收标准
    RMSE_THRESHOLD = 1e-6
    RELATIVE_THRESHOLD = 0.01  # 1%

    if rmse < RMSE_THRESHOLD and max_relative_error < RELATIVE_THRESHOLD:
        print(f"\n✅ 验证通过：HLS输出与Golden一致")
        print(f"  RMSE={rmse:.10e} < {RMSE_THRESHOLD}")
        print(
            f"  最大相对误差={max_relative_error*100:.2f}% < {RELATIVE_THRESHOLD*100}%"
        )
        return True
    else:
        print(f"\n❌ 验证失败：HLS输出与Golden不一致")
        print(f"  RMSE={rmse:.10e} > {RMSE_THRESHOLD} 或")
        print(
            f"  最大相对误差={max_relative_error*100:.2f}% > {RELATIVE_THRESHOLD*100}%"
        )

        # 打印差异较大的样本
        print(f"\n差异样本（相对误差 > 1%）：")
        large_errors_idx = np.where(relative_errors > 0.01)[0]
        for idx in large_errors_idx[:10]:
            print(
                f"  idx={idx}: Golden={golden_compare[idx]:.6f}, HLS={hls_array[idx]:.6f}, 相对误差={relative_errors[idx]*100:.2f}%"
            )

        return False


def main():
    parser = argparse.ArgumentParser(description="验证HLS输出正确性")
    parser.add_argument("--golden", required=True, help="Golden参考BIN文件路径")
    parser.add_argument(
        "--hls_output_len",
        type=int,
        default=289,
        help="HLS输出数据长度（默认289=17×17）",
    )
    args = parser.parse_args()

    # 验证Golden文件存在
    if not os.path.exists(args.golden):
        print(f"错误: Golden文件不存在 {args.golden}")
        return

    # 执行验证
    success = verify_hls_output(args.golden, args.hls_output_len)

    if success:
        print(f"\n验证完成：HLS输出信任链环节3验证通过 ✅")
    else:
        print(f"\n验证完成：HLS输出信任链环节3验证失败 ❌")


if __name__ == "__main__":
    main()
