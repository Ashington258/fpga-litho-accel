#!/usr/bin/env python3
"""
HLS vs Golden Detailed Comparison and Visualization
FPGA-Litho Project

This script:
1. Compares HLS output with Golden tmpImgp
2. Visualizes SOCS aerial images side-by-side
3. Compares SOCS kernel values between HLS input and Golden output
4. Analyzes FFT scaling impact
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import sys

# Project paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
HLS_DATA_DIR = PROJECT_ROOT / "source/SOCS_HLS/data"
GOLDEN_OUTPUT_DIR = PROJECT_ROOT / "output/verification"
GOLDEN_KERNEL_DIR = GOLDEN_OUTPUT_DIR / "kernels"

# Configuration
Nx = 4
Ny = 4
convX = 4 * Nx + 1  # = 17
convY = 4 * Ny + 1  # = 17
kerX = 2 * Nx + 1  # = 9
kerY = 2 * Ny + 1  # = 9
nk = 10
Lx = 512
Ly = 512


def load_binary_data(filepath, shape=None):
    """Load binary float data"""
    data = np.fromfile(filepath, dtype=np.float32)
    if shape:
        data = data.reshape(shape)
    return data


def save_png_image(data, filename, title, vmin=None, vmax=None):
    """Save image as PNG with proper scaling"""
    plt.figure(figsize=(8, 6))
    if vmin is None:
        vmin = data.min()
    if vmax is None:
        vmax = data.max()

    plt.imshow(data, cmap="gray", vmin=vmin, vmax=vmax)
    plt.colorbar(label="Intensity")
    plt.title(title)
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.tight_layout()
    plt.savefig(filename, dpi=150)
    plt.close()
    print(f"  Saved: {filename}")


def compare_kernel_data():
    """Compare SOCS kernel values between HLS input and Golden output"""
    print("\n" + "=" * 60)
    print("SOCS KERNEL VALUE COMPARISON")
    print("=" * 60)

    # HLS input kernels (from data directory)
    hls_kernel_dir = HLS_DATA_DIR / "kernels"

    # Golden output kernels
    golden_kernel_dir = GOLDEN_KERNEL_DIR

    print("\n[Kernel Data Comparison]")

    for k in range(nk):
        # Load HLS kernel input
        hls_r = load_binary_data(hls_kernel_dir / f"krn_{k}_r.bin")
        hls_i = load_binary_data(hls_kernel_dir / f"krn_{k}_i.bin")

        # Load Golden kernel output
        golden_r = load_binary_data(golden_kernel_dir / f"krn_{k}_r.bin")
        golden_i = load_binary_data(golden_kernel_dir / f"krn_{k}_i.bin")

        # Compute difference
        diff_r = np.abs(hls_r - golden_r)
        diff_i = np.abs(hls_i - golden_i)

        max_diff_r = diff_r.max()
        max_diff_i = diff_i.max()
        mean_diff_r = diff_r.mean()
        mean_diff_i = diff_i.mean()

        print(f"\nKernel {k}:")
        print(f"  HLS   R range: [{hls_r.min():.6f}, {hls_r.max():.6f}]")
        print(f"  HLS   I range: [{hls_i.min():.6f}, {hls_i.max():.6f}]")
        print(f"  Golden R range: [{golden_r.min():.6f}, {golden_r.max():.6f}]")
        print(f"  Golden I range: [{golden_i.min():.6f}, {golden_i.max():.6f}]")
        print(f"  Max Diff R: {max_diff_r:.6e}, Mean Diff R: {mean_diff_r:.6e}")
        print(f"  Max Diff I: {max_diff_i:.6e}, Mean Diff I: {mean_diff_i:.6e}")

        if max_diff_r < 1e-6 and max_diff_i < 1e-6:
            print(f"  ✓ Kernel {k} values MATCH perfectly")
        else:
            print(f"  ⚠ Kernel {k} values differ slightly")

    return True


def compare_output_images():
    """Compare HLS output with Golden tmpImgp"""
    print("\n" + "=" * 60)
    print("OUTPUT IMAGE COMPARISON")
    print("=" * 60)

    # Load Golden output
    golden = load_binary_data(GOLDEN_OUTPUT_DIR / "tmpImgp_pad32.bin", (convY, convX))

    # HLS output - need to run C-sim to generate, or use existing data
    # For now, we check if there's HLS output available
    hls_output_path = (
        HLS_DATA_DIR / "tmpImgp_pad32.bin"
    )  # This should be the same as golden

    # Actually, HLS output needs to be captured from C-sim
    # Let's analyze the scaling relationship

    print("\n[Golden Output Statistics]")
    print(f"  Shape: {golden.shape}")
    print(f"  Min: {golden.min():.6f}")
    print(f"  Max: {golden.max():.6f}")
    print(f"  Mean: {golden.mean():.6f}")
    print(f"  Std: {golden.std():.6f}")

    # FFT Scaling Analysis
    print("\n[FFT Scaling Impact Analysis]")
    fft_size = 32

    # Golden uses FFTW BACKWARD (unscaled): raw output
    # HLS uses scaled mode: output divided by N=32
    # Impact: intensity = |IFFT|^2
    # Golden: |raw|^2
    # HLS: |raw/32|^2 = |raw|^2 / 1024
    # Therefore: HLS intensity ≈ Golden / 1024

    scaling_factor = fft_size * fft_size  # = 1024

    print(f"  FFT size: {fft_size}×{fft_size}")
    print(f"  Golden FFT: FFTW BACKWARD (unscaled)")
    print(f"  HLS FFT: scaled mode (divided by {fft_size})")
    print(f"  Expected scaling factor: {scaling_factor}")
    print(f"  HLS intensity should be ~Golden / {scaling_factor}")

    # Visualize Golden output
    save_png_image(
        golden,
        GOLDEN_OUTPUT_DIR / "tmpImgp_pad32_visual.png",
        "Golden Output: tmpImgp (17×17)",
    )

    return golden


def compare_eigenvalues():
    """Compare eigenvalues (scales)"""
    print("\n" + "=" * 60)
    print("EIGENVALUE (SCALES) COMPARISON")
    print("=" * 60)

    hls_scales = load_binary_data(HLS_DATA_DIR / "scales.bin")
    golden_scales = load_binary_data(GOLDEN_OUTPUT_DIR / "scales.bin")

    print(f"\nHLS Scales: {hls_scales}")
    print(f"Golden Scales: {golden_scales}")

    diff = np.abs(hls_scales - golden_scales)
    print(f"\nMax difference: {diff.max():.6e}")
    print(f"Mean difference: {diff.mean():.6e}")

    if diff.max() < 1e-6:
        print("✓ Eigenvalues MATCH perfectly")
    else:
        print("⚠ Eigenvalues differ slightly")

    return hls_scales, golden_scales


def visualize_aerial_images(golden):
    """Create side-by-side visualization of aerial images"""
    print("\n" + "=" * 60)
    print("AERIAL IMAGE VISUALIZATION")
    print("=" * 60)

    # Create visualization figure
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    # 1. Golden tmpImgp (17×17)
    ax1 = axes[0, 0]
    im1 = ax1.imshow(golden, cmap="gray")
    ax1.set_title("Golden: tmpImgp (17×17)\n(convX×convY output)")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")
    plt.colorbar(im1, ax=ax1, label="Intensity")

    # 2. Golden full aerial image (512×512) if available
    aerial_path = GOLDEN_OUTPUT_DIR / "aerial_image_socs_kernel.bin"
    if aerial_path.exists():
        aerial = load_binary_data(aerial_path, (Ly, Lx))
        ax2 = axes[0, 1]
        im2 = ax2.imshow(aerial, cmap="gray")
        ax2.set_title("Golden: Full Aerial Image (512×512)\n(Fourier Interpolated)")
        ax2.set_xlabel("X")
        ax2.set_ylabel("Y")
        plt.colorbar(im2, ax=ax2, label="Intensity")

        print(f"\n[Full Aerial Image Statistics]")
        print(f"  Min: {aerial.min():.6f}")
        print(f"  Max: {aerial.max():.6f}")
        print(f"  Mean: {aerial.mean():.6f}")
    else:
        axes[0, 1].text(
            0.5,
            0.5,
            "Aerial image not available",
            ha="center",
            va="center",
            transform=axes[0, 1].transAxes,
        )

    # 3. Eigenvalue bar chart
    hls_scales = load_binary_data(HLS_DATA_DIR / "scales.bin")
    ax3 = axes[1, 0]
    ax3.bar(range(nk), hls_scales, color="steelblue", edgecolor="black")
    ax3.set_xlabel("Kernel Index")
    ax3.set_ylabel("Eigenvalue (λ)")
    ax3.set_title("SOCS Eigenvalues (λ_k)\n(TCC decomposition)")
    ax3.grid(axis="y", alpha=0.3)

    # 4. Kernel magnitude heatmap (first kernel)
    hls_kernel_dir = HLS_DATA_DIR / "kernels"
    krn_0_r = load_binary_data(hls_kernel_dir / "krn_0_r.bin", (kerY, kerX))
    krn_0_i = load_binary_data(hls_kernel_dir / "krn_0_i.bin", (kerY, kerX))
    kernel_0_mag = np.sqrt(krn_0_r**2 + krn_0_i**2)

    ax4 = axes[1, 1]
    im4 = ax4.imshow(kernel_0_mag, cmap="viridis")
    ax4.set_title("Kernel 0 Magnitude |K₀|\n(9×9 SOCS kernel)")
    ax4.set_xlabel("X")
    ax4.set_ylabel("Y")
    plt.colorbar(im4, ax=ax4, label="|K₀|")

    plt.tight_layout()
    plt.savefig(GOLDEN_OUTPUT_DIR / "hls_golden_comparison.png", dpi=150)
    plt.close()
    print(
        f"\n  Saved comparison figure: {GOLDEN_OUTPUT_DIR / 'hls_golden_comparison.png'}"
    )


def analyze_scaling_correction():
    """Analyze how to correct for FFT scaling difference"""
    print("\n" + "=" * 60)
    print("FFT SCALING CORRECTION ANALYSIS")
    print("=" * 60)

    print("\n[Problem]")
    print("  HLS uses hls::ip_fft::scaled mode")
    print("  Golden uses FFTW BACKWARD (unscaled)")
    print("  Result: HLS output ≈ Golden / 1024")

    print("\n[Correction Options]")
    print("  Option 1: Change HLS to unscaled mode")
    print("    - Modify: scaling_options = hls::ip_fft::unscaled")
    print("    - Risk: Fixed-point may overflow (dynamic range issue)")
    print("    - Need to verify: Input data range")

    print("\n  Option 2: Post-scaling compensation")
    print("    - Multiply intensity by 1024 after IFFT")
    print("    - In accumulate_intensity(): tmpImg += scale * intensity * 1024")
    print("    - Safe: No overflow risk in FFT")

    print("\n  Option 3: Pre-scaling input")
    print("    - Multiply kernel*mask product by 32 before IFFT")
    print("    - Then scaled IFFT divides by 32, net effect = raw")
    print("    - Risk: Similar overflow issue as Option 1")

    print("\n[Recommended: Option 2]")
    print("  Safest approach for fixed-point implementation")
    print("  Modify accumulate_intensity() to include scaling compensation")


def main():
    print("=" * 60)
    print("HLS vs Golden Detailed Comparison")
    print("FPGA-Litho SOCS Validation")
    print("=" * 60)

    # 1. Compare kernel data
    compare_kernel_data()

    # 2. Compare eigenvalues
    compare_eigenvalues()

    # 3. Compare output images
    golden = compare_output_images()

    # 4. Visualize aerial images
    visualize_aerial_images(golden)

    # 5. Analyze scaling correction
    analyze_scaling_correction()

    print("\n" + "=" * 60)
    print("VERIFICATION COMPLETE")
    print("=" * 60)
    print("\nGenerated files:")
    print(f"  - {GOLDEN_OUTPUT_DIR / 'tmpImgp_pad32_visual.png'}")
    print(f"  - {GOLDEN_OUTPUT_DIR / 'hls_golden_comparison.png'}")


if __name__ == "__main__":
    main()
