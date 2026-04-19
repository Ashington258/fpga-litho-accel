#!/usr/bin/env python3
"""
HLS Output vs Golden Reference Comparison

Critical Understanding:
- HLS Output: tmpImgp (17×17, calcSOCS intermediate step)
- Golden tmpImgp_pad32.bin: Same stage data, correct reference
- Golden aerial_image_socs_kernel.bin (512×512): After Fourier Interpolation, WRONG comparison target!

FFT Scaling Analysis:
1. calcSOCS uses FFTW Complex-to-Complex BACKWARD (unscaled, no 1/N)
2. Fourier Interpolation uses FFTW C2R (scaled, multiplies by fftSizeX×fftSizeY)
3. HLS matches calcSOCS behavior, so should compare with tmpImgp_pad32.bin
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def load_hls_output(output_path: Path) -> np.ndarray:
    """Load HLS board output (17×17 floats)"""
    if not output_path.exists():
        raise FileNotFoundError(f"HLS output not found: {output_path}")

    data = np.fromfile(str(output_path), dtype=np.float32)
    expected_size = 17 * 17  # convX × convY

    if len(data) != expected_size:
        print(
            f"[WARNING] HLS output size mismatch: got {len(data)}, expected {expected_size}"
        )
        # Try to handle different sizes
        if len(data) > expected_size:
            print(f"[INFO] Truncating to {expected_size} elements")
            data = data[:expected_size]
        else:
            print(f"[ERROR] Data too small, padding with zeros")
            data = np.pad(data, (0, expected_size - len(data)))

    return data.reshape(17, 17)


def load_golden_tmpImgp(golden_path: Path) -> np.ndarray:
    """Load Golden tmpImgp_pad32.bin (17×17 floats)"""
    if not golden_path.exists():
        raise FileNotFoundError(f"Golden tmpImgp not found: {golden_path}")

    data = np.fromfile(str(golden_path), dtype=np.float32)
    expected_size = 17 * 17

    if len(data) != expected_size:
        print(
            f"[WARNING] Golden size mismatch: got {len(data)}, expected {expected_size}"
        )

    return data.reshape(17, 17)


def compare_outputs(hls: np.ndarray, golden: np.ndarray, output_dir: Path):
    """Compare HLS output with Golden reference"""

    print("\n" + "=" * 60)
    print("HLS Output vs Golden tmpImgp Comparison")
    print("=" * 60)

    # Statistics
    print("\n[Statistics]")
    print(f"  HLS Output:")
    print(f"    Mean:   {np.mean(hls):.6e}")
    print(f"    Std:    {np.std(hls):.6e}")
    print(f"    Min:    {np.min(hls):.6e}")
    print(f"    Max:    {np.max(hls):.6e}")
    print(f"    Range:  {np.max(hls) - np.min(hls):.6e}")

    print(f"  Golden tmpImgp:")
    print(f"    Mean:   {np.mean(golden):.6e}")
    print(f"    Std:    {np.std(golden):.6e}")
    print(f"    Min:    {np.min(golden):.6e}")
    print(f"    Max:    {np.max(golden):.6e}")
    print(f"    Range:  {np.max(golden) - np.min(golden):.6e}")

    # Ratio analysis
    hls_mean = np.mean(np.abs(hls))
    golden_mean = np.mean(np.abs(golden))

    if hls_mean > 0 and golden_mean > 0:
        ratio = golden_mean / hls_mean
        print(f"\n[Ratio Analysis]")
        print(f"  Golden/HLS ratio: {ratio:.2f}")
        print(f"  Expected ratio:   ~1.0 (if FFT scaling correct)")
        print(f"  1024x ratio would indicate FFT scaling mismatch")
        print(f"  262144x ratio would indicate comparing with aerial image (wrong!)")

    # Difference analysis
    diff = hls - golden
    abs_diff = np.abs(diff)

    print(f"\n[Difference Analysis]")
    print(f"  Max absolute diff: {np.max(abs_diff):.6e}")
    print(f"  Mean absolute diff: {np.mean(abs_diff):.6e}")
    print(f"  Relative diff (mean): {np.mean(abs_diff) / max(golden_mean, 1e-10):.6e}")

    # Visualization
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    # HLS output
    im1 = axes[0, 0].imshow(hls, cmap="hot", interpolation="nearest")
    axes[0, 0].set_title("HLS Board Output (tmpImgp)")
    axes[0, 0].set_xlabel("X")
    axes[0, 0].set_ylabel("Y")
    plt.colorbar(im1, ax=axes[0, 0])

    # Golden tmpImgp
    im2 = axes[0, 1].imshow(golden, cmap="hot", interpolation="nearest")
    axes[0, 1].set_title("Golden tmpImgp_pad32")
    axes[0, 1].set_xlabel("X")
    axes[0, 1].set_ylabel("Y")
    plt.colorbar(im2, ax=axes[0, 1])

    # Difference
    im3 = axes[1, 0].imshow(diff, cmap="RdBu", interpolation="nearest")
    axes[1, 0].set_title("Difference (HLS - Golden)")
    axes[1, 0].set_xlabel("X")
    axes[1, 0].set_ylabel("Y")
    plt.colorbar(im3, ax=axes[1, 0])

    # Ratio map (element-wise)
    ratio_map = np.divide(hls, golden, out=np.zeros_like(hls), where=golden != 0)
    im4 = axes[1, 1].imshow(
        ratio_map, cmap="viridis", interpolation="nearest", vmin=0, vmax=2
    )
    axes[1, 1].set_title("Element-wise Ratio (HLS/Golden)")
    axes[1, 1].set_xlabel("X")
    axes[1, 1].set_ylabel("Y")
    plt.colorbar(im4, ax=axes[1, 1])

    plt.tight_layout()
    output_file = output_dir / "hls_vs_golden_comparison.png"
    plt.savefig(str(output_file), dpi=150)
    print(f"\n[Visualization saved: {output_file}]")

    # Pass/Fail criterion
    rel_diff_threshold = 0.05  # 5% tolerance
    rel_diff = np.mean(abs_diff) / max(golden_mean, 1e-10)

    if rel_diff < rel_diff_threshold:
        print(f"\n[PASS] Relative difference {rel_diff:.4e} < {rel_diff_threshold}")
        return True
    else:
        print(f"\n[FAIL] Relative difference {rel_diff:.4e} >= {rel_diff_threshold}")
        return False


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Compare HLS output with Golden tmpImgp"
    )
    parser.add_argument(
        "--hls",
        type=str,
        default="output/board/hls_output.bin",
        help="HLS board output file path",
    )
    parser.add_argument(
        "--golden",
        type=str,
        default="output/verification_original/tmpImgp_pad32.bin",
        help="Golden tmpImgp reference file path",
    )
    parser.add_argument(
        "--output",
        type=str,
        default="output/board",
        help="Output directory for comparison results",
    )

    args = parser.parse_args()

    hls_path = PROJECT_ROOT / args.hls
    golden_path = PROJECT_ROOT / args.golden
    output_dir = PROJECT_ROOT / args.output
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"[Loading data]")
    print(f"  HLS:    {hls_path}")
    print(f"  Golden: {golden_path}")

    try:
        hls_data = load_hls_output(hls_path)
        golden_data = load_golden_tmpImgp(golden_path)

        passed = compare_outputs(hls_data, golden_data, output_dir)
        return 0 if passed else 1

    except FileNotFoundError as e:
        print(f"[ERROR] {e}")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
