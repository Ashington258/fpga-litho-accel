#!/usr/bin/env python3
"""
BIN <-> PNG Converter for K-Litho project

支持功能：
1. 单文件转换：bin2png / png2bin / analyze
2. 目录批量转换：根据默认模式或指定模式，轮询目录下所有 png/bin 文件并转换
3. 输出文件直接生成在原目录中

Usage:
    # 单文件 BIN -> PNG
    python bin_png_converter.py bin2png input.bin output.png --size 1024 1024

    # 单文件 PNG -> BIN
    python bin_png_converter.py png2bin input.png output.bin

    # 分析 BIN
    python bin_png_converter.py analyze input.bin --size 1024 1024

    # 批量转换目录（使用默认模式）
    python bin_png_converter.py batch ./data

    # 批量转换目录（显式指定模式）
    python bin_png_converter.py batch ./data --mode png2bin
    python src/tool/png_bin.py batch src/matlab/mask --mode png2bin
    python src/tool/png_bin.py batch src/matlab/mask --mode bin2png
    python bin_png_converter.py batch ./data --mode bin2png --size 1024 1024
"""

import argparse
import struct
import numpy as np
from PIL import Image
import os
from pathlib import Path


# =========================
# 默认配置：你可以直接改这里
# =========================
DEFAULT_MODE = "png2bin"  # 可选: 'png2bin' 或 'bin2png'
DEFAULT_COLORMAP = "grayscale"
DEFAULT_NORMALIZE_BIN2PNG = True
DEFAULT_NORMALIZE_PNG2BIN = True


def read_bin_file(filepath, expected_size=None):
    """
    Read binary file containing float32 values.
    """
    with open(filepath, "rb") as f:
        data = f.read()

    num_floats = len(data) // 4
    values = struct.unpack(f"<{num_floats}f", data)
    arr = np.array(values, dtype=np.float32)

    if expected_size:
        height, width = expected_size
        expected_elements = height * width
        if num_floats != expected_elements:
            print(
                f"Warning: File has {num_floats} elements, expected {expected_elements}"
            )
        arr = arr.reshape((height, width))

    return arr


def write_bin_file(filepath, arr):
    """
    Write numpy array to binary file as float32 values.
    """
    arr = np.asarray(arr, dtype=np.float32)
    data = struct.pack(f"<{arr.size}f", *arr.flatten())

    with open(filepath, "wb") as f:
        f.write(data)

    print(f"Written {arr.size} float32 values ({len(data)} bytes) to {filepath}")


def analyze_bin_file(filepath, size=None):
    """
    Analyze a binary file and print statistics.
    """
    file_size = os.path.getsize(filepath)
    num_floats = file_size // 4

    print(f"\n{'=' * 60}")
    print(f"File Analysis: {filepath}")
    print(f"{'=' * 60}")
    print(f"File size: {file_size} bytes")
    print(f"Number of float32 values: {num_floats}")

    sqrt_val = int(np.sqrt(num_floats))
    if sqrt_val * sqrt_val == num_floats:
        print(f"Possible square dimension: {sqrt_val} x {sqrt_val}")

    arr = read_bin_file(filepath, size)

    print(f"\nData Statistics:")
    print(f"  Min value:  {arr.min():.6f}")
    print(f"  Max value:  {arr.max():.6f}")
    print(f"  Mean value: {arr.mean():.6f}")
    print(f"  Std dev:    {arr.std():.6f}")

    unique_vals = np.unique(arr)
    if len(unique_vals) <= 10:
        print(f"\nUnique values ({len(unique_vals)}): {unique_vals}")

    return arr


def bin_to_png(bin_path, png_path, size=None, colormap="grayscale", normalize=True):
    """
    Convert binary float array to PNG image.
    """
    arr = read_bin_file(bin_path, size)

    if size is None:
        total = arr.size
        side = int(np.sqrt(total))
        if side * side == total:
            arr = arr.reshape((side, side))
            print(f"Inferred size: {side} x {side}")
        else:
            raise ValueError(
                f"Cannot infer size for {total} elements. Please specify --size"
            )

    if normalize:
        min_val, max_val = arr.min(), arr.max()
        if min_val != max_val:
            arr_normalized = (arr - min_val) / (max_val - min_val) * 255
        else:
            arr_normalized = np.zeros_like(arr)
        arr_uint8 = arr_normalized.astype(np.uint8)
    else:
        arr_uint8 = np.clip(arr, 0, 255).astype(np.uint8)

    if colormap == "grayscale":
        img = Image.fromarray(arr_uint8, mode="L")
    else:
        try:
            import matplotlib.pyplot as plt

            cmap = plt.get_cmap(colormap)
            arr_colored = cmap(arr_uint8 / 255.0)
            arr_rgba = (arr_colored * 255).astype(np.uint8)
            img = Image.fromarray(arr_rgba, mode="RGBA")
        except ImportError:
            print("matplotlib not available, using grayscale")
            img = Image.fromarray(arr_uint8, mode="L")

    img.save(png_path)
    print(f"Converted {bin_path} -> {png_path}")
    print(f"  Size: {arr.shape[0]} x {arr.shape[1]}")
    print(f"  Data range: [{arr.min():.6f}, {arr.max():.6f}]")

    return arr


def png_to_bin(png_path, bin_path, normalize_to_01=True):
    """
    Convert PNG image to binary float array.
    """
    img = Image.open(png_path)
    arr = np.array(img, dtype=np.float32)

    print(f"Image info:")
    print(f"  Mode: {img.mode}")
    print(f"  Size: {img.size[0]} x {img.size[1]}")
    print(f"  Shape: {arr.shape}")

    if len(arr.shape) == 3:
        if arr.shape[2] == 4:  # RGBA
            arr = arr[:, :, 0]
        elif arr.shape[2] == 3:  # RGB
            arr = arr[:, :, 0]

    if normalize_to_01:
        arr = arr / 255.0

    write_bin_file(bin_path, arr)
    print(f"Converted {png_path} -> {bin_path}")

    return arr


def batch_convert(
    directory,
    mode=DEFAULT_MODE,
    size=None,
    colormap=DEFAULT_COLORMAP,
    normalize_bin2png=DEFAULT_NORMALIZE_BIN2PNG,
    normalize_png2bin=DEFAULT_NORMALIZE_PNG2BIN,
):
    """
    Batch convert all matching files in a directory.
    Output files are generated in the same directory.
    """
    directory = Path(directory)

    if not directory.exists():
        raise FileNotFoundError(f"Directory not found: {directory}")
    if not directory.is_dir():
        raise NotADirectoryError(f"Not a directory: {directory}")

    if mode == "png2bin":
        input_files = sorted(directory.glob("*.png"))
        if not input_files:
            print(f"No .png files found in {directory}")
            return

        print(f"Batch mode: png2bin")
        print(f"Directory: {directory}")
        print(f"Found {len(input_files)} PNG files")

        for png_file in input_files:
            bin_file = png_file.with_suffix(".bin")
            try:
                png_to_bin(
                    str(png_file), str(bin_file), normalize_to_01=normalize_png2bin
                )
            except Exception as e:
                print(f"Failed: {png_file} -> {bin_file}, error: {e}")

    elif mode == "bin2png":
        input_files = sorted(directory.glob("*.bin"))
        if not input_files:
            print(f"No .bin files found in {directory}")
            return

        print(f"Batch mode: bin2png")
        print(f"Directory: {directory}")
        print(f"Found {len(input_files)} BIN files")

        for bin_file in input_files:
            png_file = bin_file.with_suffix(".png")
            try:
                bin_to_png(
                    str(bin_file),
                    str(png_file),
                    size=size,
                    colormap=colormap,
                    normalize=normalize_bin2png,
                )
            except Exception as e:
                print(f"Failed: {bin_file} -> {png_file}, error: {e}")

    else:
        raise ValueError(f"Unsupported mode: {mode}")


def main():
    parser = argparse.ArgumentParser(
        description="Convert between binary float arrays (.bin) and PNG images (.png)"
    )
    subparsers = parser.add_subparsers(dest="command", help="Commands")

    # BIN to PNG command
    bin2png_parser = subparsers.add_parser("bin2png", help="Convert BIN to PNG")
    bin2png_parser.add_argument("input", help="Input BIN file")
    bin2png_parser.add_argument("output", help="Output PNG file")
    bin2png_parser.add_argument(
        "--size",
        type=int,
        nargs=2,
        default=None,
        metavar=("HEIGHT", "WIDTH"),
        help="Size of the array (height width)",
    )
    bin2png_parser.add_argument(
        "--colormap",
        default=DEFAULT_COLORMAP,
        choices=["grayscale", "viridis", "plasma", "inferno", "magma", "cividis"],
        help="Colormap for output image",
    )
    bin2png_parser.add_argument(
        "--no-normalize", action="store_true", help="Do not normalize values to 0-255"
    )

    # PNG to BIN command
    png2bin_parser = subparsers.add_parser("png2bin", help="Convert PNG to BIN")
    png2bin_parser.add_argument("input", help="Input PNG file")
    png2bin_parser.add_argument("output", help="Output BIN file")
    png2bin_parser.add_argument(
        "--no-normalize",
        action="store_true",
        help="Do not normalize pixel values to 0.0-1.0",
    )

    # Analyze command
    analyze_parser = subparsers.add_parser("analyze", help="Analyze BIN file")
    analyze_parser.add_argument("input", help="Input BIN file")
    analyze_parser.add_argument(
        "--size",
        type=int,
        nargs=2,
        default=None,
        metavar=("HEIGHT", "WIDTH"),
        help="Expected size of the array (height width)",
    )

    # Batch command
    batch_parser = subparsers.add_parser(
        "batch", help="Batch convert files in a directory"
    )
    batch_parser.add_argument("directory", help="Directory to scan")
    batch_parser.add_argument(
        "--mode",
        default=DEFAULT_MODE,
        choices=["png2bin", "bin2png"],
        help=f"Batch mode, default is {DEFAULT_MODE}",
    )
    batch_parser.add_argument(
        "--size",
        type=int,
        nargs=2,
        default=None,
        metavar=("HEIGHT", "WIDTH"),
        help="Required for bin2png when size cannot be inferred",
    )
    batch_parser.add_argument(
        "--colormap",
        default=DEFAULT_COLORMAP,
        choices=["grayscale", "viridis", "plasma", "inferno", "magma", "cividis"],
        help="Colormap for bin2png",
    )
    batch_parser.add_argument(
        "--no-normalize",
        action="store_true",
        help="Disable normalization in current mode",
    )

    args = parser.parse_args()

    if args.command == "bin2png":
        bin_to_png(
            args.input, args.output, args.size, args.colormap, not args.no_normalize
        )

    elif args.command == "png2bin":
        png_to_bin(args.input, args.output, not args.no_normalize)

    elif args.command == "analyze":
        analyze_bin_file(args.input, args.size)

    elif args.command == "batch":
        if args.mode == "bin2png":
            batch_convert(
                args.directory,
                mode="bin2png",
                size=args.size,
                colormap=args.colormap,
                normalize_bin2png=not args.no_normalize,
            )
        elif args.mode == "png2bin":
            batch_convert(
                args.directory, mode="png2bin", normalize_png2bin=not args.no_normalize
            )

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
