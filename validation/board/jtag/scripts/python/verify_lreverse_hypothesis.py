#!/usr/bin/env python3
"""
验证假设：输出数据被JTAG-to-AXI反序读取，需要lreverse后处理

分析方法：
  1. 读取当前hex输出（未处理反序）
  2. 应用lreverse逻辑（每批128个反序）
  3. 对比处理后的数据与Golden参考

假设成立的话，处理后应该匹配Golden（RMSE ≈ 1e-07）
"""

import struct
import numpy as np
import sys
import os


def hex_to_float_batched(hex_str, batch_size=128):
    """将 HEX 字符串按批次转换为 float，模拟 TCL 的分批读取逻辑"""
    # 移除注释行和空白
    lines = hex_str.strip().split("\n")
    hex_data = ""
    for line in lines:
        if not line.startswith("#"):
            hex_data += line.strip()

    # 每 8 个字符为一个 float32 (IEEE 754)
    # TCL格式：word0_word1_word2_...（每word 8 hex字符 = 4 bytes）
    words = hex_data.replace("_", "")

    floats = []
    for i in range(0, len(words), 8):
        hex_val = words[i : i + 8]
        if len(hex_val) == 8:
            # hex → bytes → float (big-endian，因为Vivado输出big-endian)
            int_val = int(hex_val, 16)
            bytes_val = struct.pack(">I", int_val)
            float_val = struct.unpack(">f", bytes_val)[0]
            floats.append(float_val)

    return np.array(floats, dtype=np.float32)


def apply_lreverse_per_batch(floats, batch_sizes=[128, 128, 33]):
    """应用 lreverse 逻辑：每批数据内部反序"""
    result = []
    idx = 0
    for batch_size in batch_sizes:
        if idx + batch_size <= len(floats):
            batch = floats[idx : idx + batch_size]
            # 反序该批次
            batch_reversed = batch[::-1]
            result.extend(batch_reversed)
            idx += batch_size
        else:
            # 剩余部分
            remaining = floats[idx:]
            result.extend(remaining[::-1])
            break

    return np.array(result, dtype=np.float32)


def load_golden_bin(path):
    """加载 Golden 参考数据"""
    with open(path, "rb") as f:
        data = f.read()
    floats = []
    for i in range(0, len(data), 4):
        if i + 4 <= len(data):
            float_val = struct.unpack("<f", data[i : i + 4])[0]  # little-endian
            floats.append(float_val)
    return np.array(floats, dtype=np.float32)


def main():
    print("=" * 70)
    print("验证假设：JTAG-to-AXI 反序读取问题")
    print("=" * 70)

    # 加载输出 hex
    hex_path = "e:/fpga-litho-accel/validation/board/jtag/scripts/tcl/run/aerial_image_output.hex"
    golden_path = "e:/fpga-litho-accel/source/SOCS_HLS/data/tmpImgp_pad32.bin"

    print(f"\n>>> 加载 HEX 输出: {hex_path}")
    with open(hex_path, "r") as f:
        hex_content = f.read()

    floats_original = hex_to_float_batched(hex_content)
    print(f"    原始数据: {len(floats_original)} values")
    print(f"    Range: [{floats_original.min():.6e}, {floats_original.max():.6e}]")

    # 加载 Golden
    print(f"\n>>> 加载 Golden 参考: {golden_path}")
    golden = load_golden_bin(golden_path)
    print(f"    Golden: {len(golden)} values")
    print(f"    Range: [{golden.min():.6e}, {golden.max():.6e}]")

    # 原始对比
    print("\n=== 原始对比（未处理反序）===")
    rmse_original = np.sqrt(np.mean((floats_original[:289] - golden) ** 2))
    max_error_original = np.max(np.abs(floats_original[:289] - golden))
    print(f"    RMSE: {rmse_original:.6e}")
    print(f"    Max Error: {max_error_original:.6e}")

    # 应用 lreverse 逻辑
    print("\n=== 应用 lreverse 后处理 ===")
    print("    分批逻辑：128 + 128 + 33")
    floats_corrected = apply_lreverse_per_batch(floats_original[:289])

    print(f"    修正后数据: {len(floats_corrected)} values")
    print(f"    Range: [{floats_corrected.min():.6e}, {floats_corrected.max():.6e}]")

    # 修正后对比
    print("\n=== 修正后对比 ===")
    rmse_corrected = np.sqrt(np.mean((floats_corrected - golden) ** 2))
    max_error_corrected = np.max(np.abs(floats_corrected - golden))
    avg_rel_error = np.mean(np.abs(floats_corrected - golden) / (golden + 1e-10)) * 100

    print(f"    RMSE: {rmse_corrected:.6e}")
    print(f"    Max Error: {max_error_corrected:.6e}")
    print(f"    Avg Relative Error: {avg_rel_error:.6f}%")

    # 验证结果
    print("\n" + "=" * 70)
    if rmse_corrected < 1e-05:
        print("✅ 假设成立！lreverse 后处理使数据匹配 Golden")
        print(f"   RMSE 从 {rmse_original:.6e} 降低到 {rmse_corrected:.6e}")
        print("\n>>> 下一步：修改 TCL 输出读取脚本，应用 lreverse")
    else:
        print("❌ 假设不成立，lreverse 后仍有较大误差")
        print("    可能需要其他数据排列修正方案")

    # 显示前10个值对比
    print("\n=== 前10个值对比 ===")
    print("Index | Original   | Corrected  | Golden     | Orig-Err   | Corr-Err")
    print("-" * 70)
    for i in range(10):
        orig = floats_original[i]
        corr = floats_corrected[i]
        gold = golden[i]
        orig_err = abs(orig - gold)
        corr_err = abs(corr - gold)
        print(
            f"{i:5d} | {orig:.6e} | {corr:.6e} | {gold:.6e} | {orig_err:.6e} | {corr_err:.6e}"
        )

    print("=" * 70)


if __name__ == "__main__":
    main()
