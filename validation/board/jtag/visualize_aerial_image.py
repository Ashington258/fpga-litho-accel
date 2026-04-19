#!/usr/bin/env python3
"""
SOCS HLS 输出可视化与对比分析

用途：
  - 从 Vivado TCL 输出的 HEX 文件读取数据
  - 与 Golden 参考（tmpImgp_pad32.bin）对比
  - 可视化 17×17 空中像结果
  - 输出误差分析报告

使用方法：
  python visualize_aerial_image.py [--output aerial_image_output.hex] [--golden tmpImgp_pad32.bin]
"""

import argparse
import struct
import numpy as np
import matplotlib.pyplot as plt
import os
import sys


def hex_to_float(hex_str):
    """将 IEEE 754 hex 字符串转换为 float"""
    # 移除空格和换行
    hex_clean = hex_str.replace(" ", "").replace("\n", "").replace("_", "")

    # 每 8 个字符为一个 float32
    floats = []
    for i in range(0, len(hex_clean), 8):
        hex_val = hex_clean[i : i + 8]
        if len(hex_val) == 8:
            # hex → bytes → float
            int_val = int(hex_val, 16)
            bytes_val = struct.pack(">I", int_val)  # big-endian
            float_val = struct.unpack(">f", bytes_val)[0]
            floats.append(float_val)

    return np.array(floats, dtype=np.float32)


def read_hex_file(hex_path):
    """读取 Vivado 输出的 HEX 文件"""
    with open(hex_path, "r") as f:
        content = f.read()

    # 提取数据部分（跳过注释）
    lines = content.split("\n")
    hex_data = ""
    for line in lines:
        if not line.startswith("#") and line.strip():
            hex_data += line.strip()

    return hex_to_float(hex_data)


def read_bin_file(bin_path):
    """读取 Golden BIN 文件"""
    floats = []
    with open(bin_path, "rb") as f:
        while True:
            data = f.read(4)
            if not data:
                break
            # little-endian float
            val = struct.unpack("<f", data)[0]
            floats.append(val)

    return np.array(floats, dtype=np.float32)


def visualize_comparison(hls_output, golden_ref, hls_file, golden_file, save_dir="./"):
    """可视化 HLS 输出与 Golden 参考"""

    # 数据形状：17×17
    hls_img = hls_output.reshape(17, 17)
    golden_img = golden_ref.reshape(17, 17)

    # 计算误差
    error = hls_img - golden_img
    abs_error = np.abs(error)
    rel_error = abs_error / (np.abs(golden_img) + 1e-10)

    # 统计指标
    mse = np.mean(error**2)
    rmse = np.sqrt(mse)
    max_error = np.max(abs_error)
    mean_rel_error = np.mean(rel_error)
    nonzero_count = np.sum(np.abs(hls_img) > 1e-6)

    print("\n=== 误差分析 ===")
    print(f"    MSE:  {mse:.6e}")
    print(f"    RMSE: {rmse:.6e}")
    print(f"    Max Absolute Error: {max_error:.6e}")
    print(f"    Mean Relative Error: {mean_rel_error:.2%}")
    print(f"    Non-zero Pixels: {nonzero_count} / 289")

    # 创建可视化图
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    fig.suptitle("SOCS HLS Output vs Golden Reference", fontsize=14, fontweight="bold")

    # HLS 输出
    im1 = axes[0, 0].imshow(hls_img, cmap="hot", interpolation="nearest")
    axes[0, 0].set_title("HLS Output (17×17)")
    axes[0, 0].set_xlabel("X")
    axes[0, 0].set_ylabel("Y")
    plt.colorbar(im1, ax=axes[0, 0], label="Intensity")

    # Golden 参考
    im2 = axes[0, 1].imshow(golden_img, cmap="hot", interpolation="nearest")
    axes[0, 1].set_title("Golden Reference (17×17)")
    axes[0, 1].set_xlabel("X")
    axes[0, 1].set_ylabel("Y")
    plt.colorbar(im2, ax=axes[0, 1], label="Intensity")

    # 误差分布
    im3 = axes[0, 2].imshow(
        error,
        cmap="RdBu",
        interpolation="nearest",
        vmin=-np.max(abs_error),
        vmax=np.max(abs_error),
    )
    axes[0, 2].set_title("Error Distribution")
    axes[0, 2].set_xlabel("X")
    axes[0, 2].set_ylabel("Y")
    plt.colorbar(im3, ax=axes[0, 2], label="Error")

    # 绝对误差
    im4 = axes[1, 0].imshow(abs_error, cmap="Reds", interpolation="nearest")
    axes[1, 0].set_title("Absolute Error")
    axes[1, 0].set_xlabel("X")
    axes[1, 0].set_ylabel("Y")
    plt.colorbar(im4, ax=axes[1, 0], label="|Error|")

    # 相对误差
    im5 = axes[1, 1].imshow(
        rel_error, cmap="Oranges", interpolation="nearest", vmin=0, vmax=1
    )
    axes[1, 1].set_title("Relative Error (%)")
    axes[1, 1].set_xlabel("X")
    axes[1, 1].set_ylabel("Y")
    plt.colorbar(im5, ax=axes[1, 1], label="Relative Error")

    # 数值对比（前 10 个值）
    axes[1, 2].bar(
        range(10), hls_img.flatten()[:10], alpha=0.7, label="HLS", color="blue"
    )
    axes[1, 2].bar(
        range(10), golden_img.flatten()[:10], alpha=0.5, label="Golden", color="orange"
    )
    axes[1, 2].set_title("First 10 Values Comparison")
    axes[1, 2].set_xlabel("Index")
    axes[1, 2].set_ylabel("Value")
    axes[1, 2].legend()
    axes[1, 2].grid(True, alpha=0.3)

    plt.tight_layout()

    # 保存图片
    output_path = os.path.join(save_dir, "aerial_image_comparison.png")
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"\n✅ Visualization saved: {output_path}")

    # 保存误差报告
    report_path = os.path.join(save_dir, "error_report.txt")
    with open(report_path, "w") as f:
        f.write("=== SOCS HLS 输出误差分析报告 ===\n\n")
        f.write(f"HLS 输出文件: {hls_file}\n")
        f.write(f"Golden 参考文件: {golden_file}\n\n")
        f.write("统计指标:\n")
        f.write(f"  MSE:              {mse:.6e}\n")
        f.write(f"  RMSE:             {rmse:.6e}\n")
        f.write(f"  Max Absolute Error: {max_error:.6e}\n")
        f.write(f"  Mean Relative Error: {mean_rel_error:.2%}\n")
        f.write(f"  Non-zero Pixels:    {nonzero_count} / 289\n\n")

        # 前 10 个值详细对比
        f.write("前 10 个像素值对比:\n")
        f.write("Index | HLS Value  | Golden Value | Error     | Rel Error\n")
        f.write("------|------------|--------------|-----------|----------\n")
        for i in range(10):
            hls_val = hls_img.flatten()[i]
            golden_val = golden_img.flatten()[i]
            err = hls_val - golden_val
            rel_err = abs(err) / (abs(golden_val) + 1e-10) * 100
            f.write(
                f"{i:5d} | {hls_val:10.6f} | {golden_val:12.6f} | {err:9.6f} | {rel_err:6.2f}%\n"
            )

    print(f"✅ Error report saved: {report_path}")

    return mse, rmse, max_error


def main():
    parser = argparse.ArgumentParser(
        description="Visualize and compare SOCS HLS output"
    )
    parser.add_argument(
        "--output",
        default="aerial_image_output.hex",
        help="HLS output HEX file from Vivado",
    )
    parser.add_argument(
        "--golden",
        default="e:/fpga-litho-accel/source/SOCS_HLS/data/tmpImgp_pad32.bin",
        help="Golden reference BIN file",
    )
    parser.add_argument(
        "--save-dir", default=".", help="Directory to save visualization results"
    )
    args = parser.parse_args()

    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║         SOCS HLS Output Visualization & Comparison          ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    # 读取 HLS 输出
    print(f"\n>>> Loading HLS output: {args.output}")
    if not os.path.exists(args.output):
        print(f"❌ HLS output file not found: {args.output}")
        print(">>> Please run Vivado TCL script first to generate output")
        return

    hls_output = read_hex_file(args.output)
    print(f"    ✅ Loaded {len(hls_output)} values (expected 289 for 17×17)")

    # 检查数据大小
    if len(hls_output) != 289:
        print(f"⚠️ Warning: Expected 289 values, got {len(hls_output)}")
        print("    >>> Vivado output may be incomplete or corrupted")

    # 读取 Golden 参考
    print(f"\n>>> Loading Golden reference: {args.golden}")
    if not os.path.exists(args.golden):
        print(f"❌ Golden reference not found: {args.golden}")
        return

    golden_ref = read_bin_file(args.golden)
    print(f"    ✅ Loaded {len(golden_ref)} values (17×17)")

    # 数据统计
    print("\n=== 数据统计 ===")
    print(f"HLS Output:")
    print(f"    Min:   {np.min(hls_output):.6e}")
    print(f"    Max:   {np.max(hls_output):.6e}")
    print(f"    Mean:  {np.mean(hls_output):.6e}")
    print(f"    Std:   {np.std(hls_output):.6e}")
    print(f"    Non-zero count: {np.sum(np.abs(hls_output) > 1e-6)}")

    print(f"\nGolden Reference:")
    print(f"    Min:   {np.min(golden_ref):.6e}")
    print(f"    Max:   {np.max(golden_ref):.6e}")
    print(f"    Mean:  {np.mean(golden_ref):.6e}")
    print(f"    Std:   {np.std(golden_ref):.6e}")

    # 可视化对比
    visualize_comparison(
        hls_output, golden_ref, args.output, args.golden, args.save_dir
    )

    # 显示图片
    plt.show()

    print("\n✅ Visualization completed!")


if __name__ == "__main__":
    main()
