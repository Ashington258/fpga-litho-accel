#!/usr/bin/env python3
"""
HLS vs Golden Data Visualization and Comparison
FPGA-Litho Project - Nx=4 Configuration

This script visualizes and compares HLS simulation results with golden reference data.
Focus: FFT scaling impact, aerial image visualization, kernel value comparison.
"""

import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib

matplotlib.use("Agg")  # Non-interactive backend

# Project paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
HLS_DATA_DIR = PROJECT_ROOT / "source/SOCS_HLS/data"
HLS_BUILD_DIR = PROJECT_ROOT / "source/SOCS_HLS/hls/socs_simple_csim_v4/hls/csim/build"
VALIDATION_OUTPUT = PROJECT_ROOT / "output/verification"

# Configuration Nx=4
Nx = 4
Ny = 4
convX = 4 * Nx + 1  # 17
convY = 4 * Ny + 1  # 17
kerX = 2 * Nx + 1  # 9
kerY = 2 * Ny + 1  # 9
nk = 10


def load_binary_data(filepath):
    """Load binary float data"""
    data = np.fromfile(filepath, dtype=np.float32)
    return data


def reshape_2d(data, rows, cols):
    """Reshape 1D data to 2D array"""
    return data.reshape(rows, cols)


def main():
    print("=" * 60)
    print("HLS vs Golden Visualization and Comparison")
    print("Nx=4 Configuration (512x512 mask, 9x9 kernels)")
    print("=" * 60)

    # Load Golden reference data
    print("\n[Loading Golden Data]")
    golden_output = load_binary_data(HLS_DATA_DIR / "tmpImgp_pad32.bin")
    golden_output_2d = reshape_2d(golden_output, convY, convX)
    print(f"  Golden tmpImgp_pad32: shape {golden_output.shape}")
    print(f"  Range: [{golden_output.min():.4f}, {golden_output.max():.4f}]")
    print(f"  Mean: {golden_output.mean():.4f}")
    print(f"  Std: {golden_output.std():.4f}")

    # Load Golden kernels
    golden_kernels_r = []
    golden_kernels_i = []
    for k in range(nk):
        krn_r = load_binary_data(HLS_DATA_DIR / f"kernels/krn_{k}_r.bin")
        krn_i = load_binary_data(HLS_DATA_DIR / f"kernels/krn_{k}_i.bin")
        golden_kernels_r.append(reshape_2d(krn_r, kerY, kerX))
        golden_kernels_i.append(reshape_2d(krn_i, kerY, kerX))
    print(f"  Loaded {nk} golden kernels (9x9 each)")

    # Load Golden eigenvalues
    golden_scales = load_binary_data(HLS_DATA_DIR / "scales.bin")
    print(f"  Golden scales (eigenvalues): {golden_scales}")

    # Check if HLS output exists (would need to be captured from C-sim)
    # For now, we'll analyze the golden data characteristics
    print("\n[Golden Data Analysis]")

    # Analyze output intensity pattern
    center = golden_output_2d[convY // 2, convX // 2]
    print(f"  Center pixel value: {center:.4f}")
    print(
        f"  Peak location: row {np.argmax(golden_output_2d.max(axis=1))}, col {np.argmax(golden_output_2d.max(axis=0))}"
    )

    # Analyze kernel characteristics
    print("\n[Kernel Analysis]")
    for k in range(min(3, nk)):  # Show first 3 kernels
        krn_r = golden_kernels_r[k]
        krn_i = golden_kernels_i[k]
        krn_mag = np.sqrt(krn_r**2 + krn_i**2)
        print(f"  Kernel {k}:")
        print(
            f"    R: range [{krn_r.min():.4f}, {krn_r.max():.4f}], mean={krn_r.mean():.4f}"
        )
        print(
            f"    I: range [{krn_i.min():.4f}, {krn_i.max():.4f}], mean={krn_i.mean():.4f}"
        )
        print(
            f"    Mag: range [{krn_mag.min():.4f}, {krn_mag.max():.4f}], center={krn_mag[kerY//2, kerX//2]:.4f}"
        )

    # Create visualization
    print("\n[Creating Visualizations]")

    # Figure 1: Golden Output Aerial Image
    fig1, ax1 = plt.subplots(figsize=(8, 6))
    im1 = ax1.imshow(golden_output_2d, cmap="hot", interpolation="nearest")
    ax1.set_title("Golden Reference: Aerial Image (Nx=4)", fontsize=14)
    ax1.set_xlabel("X (pixels)")
    ax1.set_ylabel("Y (pixels)")
    plt.colorbar(im1, ax=ax1, label="Intensity")
    plt.tight_layout()
    plt.savefig(VALIDATION_OUTPUT / "golden_aerial_image_nx4.png", dpi=150)
    print(f"  Saved: golden_aerial_image_nx4.png")
    plt.close()

    # Figure 2: Kernels visualization (first 4 kernels)
    fig2, axes2 = plt.subplots(2, 4, figsize=(16, 8))
    for k in range(min(4, nk)):
        krn_r = golden_kernels_r[k]
        krn_i = golden_kernels_i[k]
        krn_mag = np.sqrt(krn_r**2 + krn_i**2)

        # Real part
        ax_r = axes2[0, k]
        im_r = ax_r.imshow(krn_r, cmap="RdBu", interpolation="nearest")
        ax_r.set_title(f"Kernel {k} Real\nλ={golden_scales[k]:.2f}", fontsize=12)
        plt.colorbar(im_r, ax=ax_r)

        # Magnitude
        ax_mag = axes2[1, k]
        im_mag = ax_mag.imshow(krn_mag, cmap="viridis", interpolation="nearest")
        ax_mag.set_title(f"Kernel {k} Magnitude", fontsize=12)
        plt.colorbar(im_mag, ax=ax_mag)

    plt.tight_layout()
    plt.savefig(VALIDATION_OUTPUT / "golden_kernels_nx4.png", dpi=150)
    print(f"  Saved: golden_kernels_nx4.png")
    plt.close()

    # Figure 3: Eigenvalue distribution
    fig3, ax3 = plt.subplots(figsize=(10, 6))
    ax3.bar(range(nk), golden_scales, color="steelblue", alpha=0.7)
    ax3.set_xlabel("Kernel Index", fontsize=12)
    ax3.set_ylabel("Eigenvalue (λ)", fontsize=12)
    ax3.set_title("SOCS Eigenvalues (Nx=4 Configuration)", fontsize=14)
    ax3.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(VALIDATION_OUTPUT / "golden_eigenvalues_nx4.png", dpi=150)
    print(f"  Saved: golden_eigenvalues_nx4.png")
    plt.close()

    # Figure 4: Output profile analysis
    fig4, axes4 = plt.subplots(1, 2, figsize=(12, 5))

    # Horizontal profile through center
    ax_h = axes4[0]
    ax_h.plot(
        range(convX),
        golden_output_2d[convY // 2, :],
        "b-",
        linewidth=2,
        label="Horizontal",
    )
    ax_h.set_xlabel("X position", fontsize=12)
    ax_h.set_ylabel("Intensity", fontsize=12)
    ax_h.set_title("Horizontal Profile (Center Row)", fontsize=14)
    ax_h.grid(True, alpha=0.3)
    ax_h.legend()

    # Vertical profile through center
    ax_v = axes4[1]
    ax_v.plot(
        range(convY),
        golden_output_2d[:, convX // 2],
        "r-",
        linewidth=2,
        label="Vertical",
    )
    ax_v.set_xlabel("Y position", fontsize=12)
    ax_v.set_ylabel("Intensity", fontsize=12)
    ax_v.set_title("Vertical Profile (Center Column)", fontsize=14)
    ax_v.grid(True, alpha=0.3)
    ax_v.legend()

    plt.tight_layout()
    plt.savefig(VALIDATION_OUTPUT / "golden_profiles_nx4.png", dpi=150)
    print(f"  Saved: golden_profiles_nx4.png")
    plt.close()

    # FFT Scaling Analysis Note
    print("\n[FFT Scaling Analysis]")
    print("  HLS Configuration: hls::ip_fft::scaled (divide by N=32)")
    print("  Golden (FFTW): FFTW_BACKWARD (unscaled)")
    print("  Expected scaling factor: 32 × 32 = 1024")
    print("  If HLS output ≈ Golden / 1024, scaling is correctly applied")

    print("\n" + "=" * 60)
    print("VISUALIZATION COMPLETE")
    print("=" * 60)
    print(f"\nOutput files saved to: {VALIDATION_OUTPUT}")
    print("\nNext steps:")
    print("  1. Run HLS C-sim to capture actual HLS output")
    print("  2. Compare HLS output with golden reference")
    print("  3. Verify FFT scaling factor")


if __name__ == "__main__":
    main()
