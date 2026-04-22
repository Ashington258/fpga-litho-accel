#!/usr/bin/env python3
"""
SOCS HLS 输出完整可视化与对比分析（包含 Fourier Interpolation）

用途：
  - 从 Vivado TCL 输出的 HEX 文件读取 17×17 HLS 输出
  - Zero-padding 到 32×32
  - Fourier Interpolation (FI) 扩展到 512×512 最终空中像
  - 与 Golden SOCS 和 TCC Direct 参考 对比
  - 生成完整的误差分析报告和可视化图

使用方法：
  python visualize_aerial_image_full.py [--output aerial_image_output.hex] [--golden-dir path]

流程：
  17×17 tmpImgp (HLS输出) → 32×32 (zero-pad) → 512×512 (FI) → 最终空中像
"""

import argparse
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib

matplotlib.use("Agg")  # Non-interactive backend
import os
import sys
from pathlib import Path


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
    """读取 Golden BIN 文件 (little-endian float32)"""
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


def fftshift_2d(arr):
    """Shift zero-frequency to center (equivalent to numpy fftshift)"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, rows // 2, axis=0), cols // 2, axis=1)


def ifftshift_2d(arr):
    """Inverse of fftshift (move zero-frequency back to corner)"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, -rows // 2, axis=0), -cols // 2, axis=1)


def fourier_interpolation_numpy(input_img, output_size):
    """
    Fourier Interpolation using numpy FFT.

    Args:
        input_img: 2D array (in_sizeX x in_sizeY)
        output_size: tuple (out_sizeX, out_sizeY)

    Returns:
        output_img: 2D array (out_sizeX x out_sizeY)
    """
    in_sizeX, in_sizeY = input_img.shape
    out_sizeX, out_sizeY = output_size

    # Step 1: ifftshift to move zero-frequency to corner for FFT
    shifted_in = ifftshift_2d(input_img)

    # Step 2: Forward FFT (R2C equivalent using numpy)
    spectrum = np.fft.fft2(shifted_in)
    spectrum /= in_sizeX * in_sizeY  # Normalize

    # Step 3: Embed spectrum into larger output size
    # Copy low-frequency content (center region)
    in_wt = in_sizeX // 2 + 1  # Width of input half-spectrum
    out_wt = out_sizeX // 2 + 1  # Width of output half-spectrum

    # Create output spectrum (zeros)
    output_spectrum = np.zeros((out_sizeY, out_sizeX), dtype=np.complex128)

    # Copy positive frequency region (top half, including DC)
    posFreqRows = in_sizeY // 2 + 1
    for y in range(posFreqRows):
        for x in range(min(in_wt, out_wt)):
            output_spectrum[y, x] = spectrum[y, x]

    # Copy negative frequency region (bottom half) with offset
    difY = out_sizeY - in_sizeY
    for y in range(posFreqRows, in_sizeY):
        out_y = y + difY
        for x in range(min(in_wt, out_wt)):
            if out_y < out_sizeY:
                output_spectrum[out_y, x] = spectrum[y, x]

    # Step 4: Inverse FFT to get output image
    output_img = np.fft.ifft2(output_spectrum).real * (out_sizeX * out_sizeY)

    # Step 5: fftshift to restore zero-frequency to center
    output_img = fftshift_2d(output_img)

    return output_img


def zero_pad_center(input_img, target_size):
    """
    Zero-pad input image to target size, centered.

    Args:
        input_img: 2D array (smaller)
        target_size: tuple (target_width, target_height)

    Returns:
        padded_img: 2D array (target_size)
    """
    in_h, in_w = input_img.shape
    out_h, out_w = target_size

    padded = np.zeros((out_h, out_w), dtype=np.float64)
    offset_h = (out_h - in_h) // 2
    offset_w = (out_w - in_w) // 2

    padded[offset_h : offset_h + in_h, offset_w : offset_w + in_w] = input_img
    return padded


def visualize_full_pipeline(
    hls_17x17,
    golden_17x17,
    aerial_hls,
    aerial_golden,
    aerial_tcc,
    hls_file,
    save_dir="./",
):
    """完整可视化对比：17×17 → 32×32 → 512×512"""

    # 创建 3×4 可视化布局
    fig = plt.figure(figsize=(20, 15))
    fig.suptitle(
        "SOCS HLS Full Pipeline: tmpImgp (17×17) → FI → Aerial (512×512)",
        fontsize=16,
        fontweight="bold",
    )

    # === Row 1: 17×17 HLS 输出对比 ===
    ax1 = fig.add_subplot(3, 4, 1)
    im1 = ax1.imshow(hls_17x17, cmap="hot", interpolation="nearest")
    ax1.set_title("HLS IP Output\n(17×17 tmpImgp)")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")
    plt.colorbar(im1, ax=ax1, fraction=0.046, label="Intensity")

    ax2 = fig.add_subplot(3, 4, 2)
    im2 = ax2.imshow(golden_17x17, cmap="hot", interpolation="nearest")
    ax2.set_title("Golden tmpImgp\n(CPU Reference)")
    ax2.set_xlabel("X")
    ax2.set_ylabel("Y")
    plt.colorbar(im2, ax=ax2, fraction=0.046, label="Intensity")

    # 17×17 误差
    diff_17 = hls_17x17 - golden_17x17
    rmse_17 = np.sqrt(np.mean(diff_17**2))
    ax3 = fig.add_subplot(3, 4, 3)
    im3 = ax3.imshow(
        diff_17,
        cmap="RdBu",
        interpolation="nearest",
        vmin=-np.max(np.abs(diff_17)),
        vmax=np.max(np.abs(diff_17)),
    )
    ax3.set_title(f"17×17 Error\nRMSE={rmse_17:.2e}")
    ax3.set_xlabel("X")
    ax3.set_ylabel("Y")
    plt.colorbar(im3, ax=ax3, fraction=0.046, label="Error")

    # 17×17 数值分布
    ax4 = fig.add_subplot(3, 4, 4)
    ax4.hist(hls_17x17.flatten(), bins=30, alpha=0.7, label="HLS", color="blue")
    ax4.hist(golden_17x17.flatten(), bins=30, alpha=0.5, label="Golden", color="orange")
    ax4.set_title("17×17 Value Distribution")
    ax4.set_xlabel("Value")
    ax4.set_ylabel("Count")
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    # === Row 2: 512×512 最终空中像对比 ===
    ax5 = fig.add_subplot(3, 4, 5)
    im5 = ax5.imshow(aerial_hls, cmap="hot", interpolation="nearest")
    ax5.set_title("HLS + FI Output\n(512×512 Aerial)")
    ax5.set_xlabel("X")
    ax5.set_ylabel("Y")
    plt.colorbar(im5, ax=ax5, fraction=0.046, label="Intensity")

    ax6 = fig.add_subplot(3, 4, 6)
    im6 = ax6.imshow(aerial_golden, cmap="hot", interpolation="nearest")
    ax6.set_title("Golden SOCS\n(512×512 Aerial)")
    ax6.set_xlabel("X")
    ax6.set_ylabel("Y")
    plt.colorbar(im6, ax=ax6, fraction=0.046, label="Intensity")

    ax7 = fig.add_subplot(3, 4, 7)
    im7 = ax7.imshow(aerial_tcc, cmap="hot", interpolation="nearest")
    ax7.set_title("TCC Direct\n(Theoretical Standard)")
    ax7.set_xlabel("X")
    ax7.set_ylabel("Y")
    plt.colorbar(im7, ax=ax7, fraction=0.046, label="Intensity")

    # 512×512 误差 (HLS vs Golden SOCS)
    diff_512_hls_golden = aerial_hls - aerial_golden
    rmse_512_hls_golden = np.sqrt(np.mean(diff_512_hls_golden**2))
    ax8 = fig.add_subplot(3, 4, 8)
    im8 = ax8.imshow(np.abs(diff_512_hls_golden), cmap="Reds", interpolation="nearest")
    ax8.set_title(f"|HLS - Golden| Diff\nRMSE={rmse_512_hls_golden:.2e}")
    ax8.set_xlabel("X")
    ax8.set_ylabel("Y")
    plt.colorbar(im8, ax=ax8, fraction=0.046, label="|Error|")

    # === Row 3: 详细分析和对比 ===
    # 中心区域放大 (32×32)
    ax9 = fig.add_subplot(3, 4, 9)
    center_hls = aerial_hls[240:272, 240:272]
    im9 = ax9.imshow(center_hls, cmap="hot", interpolation="nearest")
    ax9.set_title("HLS Center (32×32)\nRegion 240-272")
    plt.colorbar(im9, ax=ax9, fraction=0.046)

    ax10 = fig.add_subplot(3, 4, 10)
    center_golden = aerial_golden[240:272, 240:272]
    im10 = ax10.imshow(center_golden, cmap="hot", interpolation="nearest")
    ax10.set_title("Golden Center (32×32)\nRegion 240-272")
    plt.colorbar(im10, ax=ax10, fraction=0.046)

    # SOCS vs TCC 误差
    diff_socs_tcc = aerial_golden - aerial_tcc
    rmse_socs_tcc = np.sqrt(np.mean(diff_socs_tcc**2))
    ax11 = fig.add_subplot(3, 4, 11)
    im11 = ax11.imshow(np.abs(diff_socs_tcc), cmap="Oranges", interpolation="nearest")
    ax11.set_title(f"|SOCS - TCC| Diff\nRMSE={rmse_socs_tcc:.2e}")
    plt.colorbar(im11, ax=ax11, fraction=0.046, label="|Error|")

    # 512×512 数值分布
    ax12 = fig.add_subplot(3, 4, 12)
    ax12.hist(aerial_hls.flatten(), bins=50, alpha=0.7, label="HLS+FI", color="blue")
    ax12.hist(
        aerial_golden.flatten(), bins=50, alpha=0.5, label="Golden SOCS", color="orange"
    )
    ax12.hist(
        aerial_tcc.flatten(), bins=50, alpha=0.3, label="TCC Direct", color="green"
    )
    ax12.set_title("512×512 Value Distribution")
    ax12.set_xlabel("Value")
    ax12.set_ylabel("Count")
    ax12.legend()
    ax12.grid(True, alpha=0.3)

    plt.tight_layout()

    # 保存图片
    output_path = os.path.join(save_dir, "aerial_image_full_pipeline.png")
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"\n✅ Visualization saved: {output_path}")

    return rmse_17, rmse_512_hls_golden, rmse_socs_tcc


def generate_error_report(
    hls_17x17,
    golden_17x17,
    aerial_hls,
    aerial_golden,
    aerial_tcc,
    hls_file,
    golden_17_file,
    save_dir="./",
):
    """生成详细的误差分析报告"""

    # 计算各项误差指标
    diff_17 = hls_17x17 - golden_17x17
    rmse_17 = np.sqrt(np.mean(diff_17**2))
    max_err_17 = np.max(np.abs(diff_17))
    rel_err_17 = np.mean(np.abs(diff_17) / (np.abs(golden_17x17) + 1e-10))

    diff_512_hls_golden = aerial_hls - aerial_golden
    rmse_512_hls_golden = np.sqrt(np.mean(diff_512_hls_golden**2))
    max_err_512_hls_golden = np.max(np.abs(diff_512_hls_golden))

    diff_512_golden_tcc = aerial_golden - aerial_tcc
    rmse_512_golden_tcc = np.sqrt(np.mean(diff_512_golden_tcc**2))

    diff_512_hls_tcc = aerial_hls - aerial_tcc
    rmse_512_hls_tcc = np.sqrt(np.mean(diff_512_hls_tcc**2))

    report_path = os.path.join(save_dir, "error_report_full.txt")
    with open(report_path, "w") as f:
        f.write("=" * 70 + "\n")
        f.write("        SOCS HLS Full Pipeline 误差分析报告\n")
        f.write("=" * 70 + "\n\n")

        f.write("数据来源:\n")
        f.write(f"  HLS 输出文件:     {hls_file}\n")
        f.write(f"  Golden tmpImgp:   {golden_17_file}\n")
        f.write(
            f"  Golden SOCS 512:  output/verification/aerial_image_socs_kernel.bin\n"
        )
        f.write(
            f"  TCC Direct 512:   output/verification/aerial_image_tcc_direct.bin\n\n"
        )

        f.write("=" * 70 + "\n")
        f.write("Stage 1: HLS IP Output (17×17 tmpImgp)\n")
        f.write("=" * 70 + "\n\n")
        f.write("统计指标:\n")
        f.write(f"  RMSE:                {rmse_17:.6e}\n")
        f.write(f"  Max Absolute Error:  {max_err_17:.6e}\n")
        f.write(f"  Mean Relative Error: {rel_err_17:.2%}\n")
        f.write(f"  Non-zero Pixels:     {np.sum(np.abs(hls_17x17) > 1e-6)} / 289\n\n")

        f.write("前 10 个像素值对比:\n")
        f.write("Index | HLS Value  | Golden Value | Error     | Rel Error\n")
        f.write("------|------------|--------------|-----------|----------\n")
        for i in range(10):
            hls_val = hls_17x17.flatten()[i]
            golden_val = golden_17x17.flatten()[i]
            err = hls_val - golden_val
            rel_err = abs(err) / (abs(golden_val) + 1e-10) * 100
            f.write(
                f"{i:5d} | {hls_val:10.6f} | {golden_val:12.6f} | {err:9.6f} | {rel_err:6.2f}%\n"
            )

        f.write("\n" + "=" * 70 + "\n")
        f.write("Stage 2: Fourier Interpolation (32×32 → 512×512)\n")
        f.write("=" * 70 + "\n\n")
        f.write("FI 参数:\n")
        f.write(f"  输入尺寸:  32×32 (zero-padded from 17×17)\n")
        f.write(f"  输出尺寸:  512×512\n")
        f.write(f"  方法:      NumPy FFT (fft2/ifft2)\n\n")

        f.write("=" * 70 + "\n")
        f.write("Stage 3: Final Aerial Image (512×512)\n")
        f.write("=" * 70 + "\n\n")
        f.write("对比结果:\n")
        f.write(f"  HLS+FI vs Golden SOCS:\n")
        f.write(f"    RMSE: {rmse_512_hls_golden:.6e}\n")
        f.write(f"    Max Error: {max_err_512_hls_golden:.6e}\n\n")

        f.write(f"  Golden SOCS vs TCC Direct:\n")
        f.write(f"    RMSE: {rmse_512_golden_tcc:.6e}\n\n")

        f.write(f"  HLS+FI vs TCC Direct (End-to-End):\n")
        f.write(f"    RMSE: {rmse_512_hls_tcc:.6e}\n\n")

        f.write("数值范围:\n")
        f.write(f"  HLS+FI:    [{aerial_hls.min():.6f}, {aerial_hls.max():.6f}]\n")
        f.write(
            f"  Golden:    [{aerial_golden.min():.6f}, {aerial_golden.max():.6f}]\n"
        )
        f.write(f"  TCC Direct: [{aerial_tcc.min():.6f}, {aerial_tcc.max():.6f}]\n\n")

        f.write("=" * 70 + "\n")
        f.write("结论:\n")
        f.write("=" * 70 + "\n\n")

        # 判断验证是否通过
        if rmse_17 < 1e-5 and rmse_512_hls_golden < 1e-5:
            f.write("✅ 验证通过！HLS IP 输出与 Golden 参考高度一致。\n")
            f.write("   17×17 RMSE < 1e-5，512×512 RMSE < 1e-5\n")
        elif rmse_17 < 1e-4:
            f.write("⚠️ 17×17 验证基本通过，但存在微小误差。\n")
            f.write(f"   建议检查 HLS IP 精度配置或数据加载流程。\n")
        else:
            f.write("❌ 验证失败！HLS IP 输出与 Golden 参考存在显著差异。\n")
            f.write(f"   请检查:\n")
            f.write(f"     1. HLS IP 配置是否正确\n")
            f.write(f"     2. 输入数据是否正确加载\n")
            f.write(f"     3. DDR 内存数据是否完整\n")

    print(f"✅ Error report saved: {report_path}")
    return report_path


def main():
    parser = argparse.ArgumentParser(
        description="Visualize and compare SOCS HLS output with full FI pipeline"
    )
    parser.add_argument(
        "--output",
        default="validation/board/jtag/scripts/tcl/run/aerial_image_output.hex",
        help="HLS output HEX file from Vivado (17×17 tmpImgp)",
    )
    parser.add_argument(
        "--golden-17",
        default="e:/fpga-litho-accel/source/SOCS_HLS/data/tmpImgp_pad32.bin",
        help="Golden tmpImgp BIN file (17×17)",
    )
    parser.add_argument(
        "--golden-512",
        default="e:/fpga-litho-accel/output/verification/aerial_image_socs_kernel.bin",
        help="Golden SOCS aerial image BIN file (512×512)",
    )
    parser.add_argument(
        "--tcc-512",
        default="e:/fpga-litho-accel/output/verification/aerial_image_tcc_direct.bin",
        help="TCC Direct aerial image BIN file (512×512)",
    )
    parser.add_argument(
        "--save-dir", default=".", help="Directory to save visualization results"
    )
    parser.add_argument(
        "--save-fi-output",
        default=None,
        help="Path to save FI output (512×512) as BIN file",
    )
    args = parser.parse_args()

    print("\n" + "=" * 70)
    print("     SOCS HLS Full Pipeline Visualization & Validation")
    print("     tmpImgp (17×17) → Zero-Pad (32×32) → FI → Aerial (512×512)")
    print("=" * 70)

    # === Step 1: 读取 HLS 输出 (17×17) ===
    print(f"\n>>> [Stage 1] Loading HLS output: {args.output}")
    if not os.path.exists(args.output):
        print(f"❌ HLS output file not found: {args.output}")
        print(">>> Please run Vivado TCL script first to generate output")
        sys.exit(1)

    hls_17x17 = read_hex_file(args.output)
    print(f"    ✅ Loaded {len(hls_17x17)} values (expected 289 for 17×17)")

    if len(hls_17x17) != 289:
        print(f"⚠️ Warning: Expected 289 values, got {len(hls_17x17)}")
        print("    >>> Vivado output may be incomplete or corrupted")
        # 尝试继续处理
        if len(hls_17x17) < 289:
            hls_17x17 = np.pad(hls_17x17, (0, 289 - len(hls_17x17)), mode="constant")
            print(f"    >>> Zero-padded to 289 values")
        else:
            hls_17x17 = hls_17x17[:289]

    hls_17x17 = hls_17x17.reshape(17, 17)

    # === Step 2: 读取 Golden tmpImgp (17×17) ===
    print(f"\n>>> Loading Golden tmpImgp: {args.golden_17}")
    if not os.path.exists(args.golden_17):
        print(f"❌ Golden tmpImgp not found: {args.golden_17}")
        sys.exit(1)

    golden_17x17 = read_bin_file(args.golden_17)
    print(f"    ✅ Loaded {len(golden_17x17)} values")
    golden_17x17 = golden_17x17.reshape(17, 17)

    # === Step 3: Zero-padding 17×17 → 32×32 ===
    print(f"\n>>> [Stage 2] Zero-padding: 17×17 → 32×32")
    hls_32x32 = zero_pad_center(hls_17x17, (32, 32))
    golden_32x32 = zero_pad_center(golden_17x17, (32, 32))
    print(f"    ✅ Zero-padded to 32×32 (offset=7)")

    # === Step 4: Fourier Interpolation 32×32 → 512×512 ===
    print(f"\n>>> [Stage 3] Fourier Interpolation: 32×32 → 512×512")
    aerial_hls = fourier_interpolation_numpy(hls_32x32, (512, 512)).astype(np.float32)
    print(f"    ✅ FI completed")
    print(f"    Output range: [{aerial_hls.min():.6f}, {aerial_hls.max():.6f}]")

    # Save FI output if requested
    if args.save_fi_output:
        aerial_hls.tofile(args.save_fi_output)
        print(f"    ✅ Saved FI output: {args.save_fi_output}")

    # === Step 5: 读取 Golden 参考 (512×512) ===
    print(f"\n>>> Loading Golden SOCS aerial: {args.golden_512}")
    if not os.path.exists(args.golden_512):
        print(f"❌ Golden SOCS aerial not found: {args.golden_512}")
        sys.exit(1)

    aerial_golden = read_bin_file(args.golden_512).reshape(512, 512)
    print(f"    ✅ Loaded 512×512 Golden SOCS aerial")

    print(f"\n>>> Loading TCC Direct aerial: {args.tcc_512}")
    if not os.path.exists(args.tcc_512):
        print(f"❌ TCC Direct aerial not found: {args.tcc_512}")
        sys.exit(1)

    aerial_tcc = read_bin_file(args.tcc_512).reshape(512, 512)
    print(f"    ✅ Loaded 512×512 TCC Direct aerial")

    # === Step 6: 数据统计 ===
    print("\n" + "-" * 70)
    print("数据统计:")
    print("-" * 70)
    print(f"  HLS tmpImgp (17×17):")
    print(f"    Min:   {hls_17x17.min():.6e}")
    print(f"    Max:   {hls_17x17.max():.6e}")
    print(f"    Mean:  {hls_17x17.mean():.6e}")
    print(f"    Std:   {hls_17x17.std():.6e}")

    print(f"\n  Golden tmpImgp (17×17):")
    print(f"    Min:   {golden_17x17.min():.6e}")
    print(f"    Max:   {golden_17x17.max():.6e}")
    print(f"    Mean:  {golden_17x17.mean():.6e}")
    print(f"    Std:   {golden_17x17.std():.6e}")

    print(f"\n  HLS+FI Aerial (512×512):")
    print(f"    Min:   {aerial_hls.min():.6e}")
    print(f"    Max:   {aerial_hls.max():.6e}")
    print(f"    Mean:  {aerial_hls.mean():.6e}")

    print(f"\n  Golden SOCS Aerial (512×512):")
    print(f"    Min:   {aerial_golden.min():.6e}")
    print(f"    Max:   {aerial_golden.max():.6e}")

    print(f"\n  TCC Direct Aerial (512×512):")
    print(f"    Min:   {aerial_tcc.min():.6e}")
    print(f"    Max:   {aerial_tcc.max():.6e}")

    # === Step 7: 可视化与报告 ===
    print("\n>>> [Stage 4] Generating visualization and error report...")

    rmse_17, rmse_512_hls_golden, rmse_socs_tcc = visualize_full_pipeline(
        hls_17x17,
        golden_17x17,
        aerial_hls,
        aerial_golden,
        aerial_tcc,
        args.output,
        args.save_dir,
    )

    generate_error_report(
        hls_17x17,
        golden_17x17,
        aerial_hls,
        aerial_golden,
        aerial_tcc,
        args.output,
        args.golden_17,
        args.save_dir,
    )

    # === Step 8: 最终验证结果 ===
    print("\n" + "=" * 70)
    print("验证结果:")
    print("=" * 70)
    print(f"  17×17 tmpImgp RMSE:      {rmse_17:.6e}")
    print(f"  512×512 Aerial RMSE:     {rmse_512_hls_golden:.6e}")
    print(f"  SOCS vs TCC RMSE:        {rmse_socs_tcc:.6e}")

    if rmse_17 < 1e-5 and rmse_512_hls_golden < 1e-5:
        print("\n✅ 验证通过！HLS IP 输出与 Golden 参考高度一致。")
    elif rmse_17 < 1e-4:
        print("\n⚠️ 17×17 验证基本通过，但存在微小误差。")
    else:
        print("\n❌ 验证失败！HLS IP 输出与 Golden 参考存在显著差异。")
        print("   请检查 HLS IP 配置、数据加载流程和 DDR 内存状态。")

    print("\n✅ Full pipeline visualization completed!")
    print("=" * 70)


if __name__ == "__main__":
    main()
