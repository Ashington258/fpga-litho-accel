#!/usr/bin/env python3
"""
Check HLS Golden Data Consistency
FPGA-Litho Project

This script checks the consistency of HLS data directory golden files.
"""

import numpy as np
from pathlib import Path
import sys

# Project paths
PROJECT_ROOT = Path(__file__).resolve().parent.parent
HLS_DATA_DIR = PROJECT_ROOT / "source/SOCS_HLS/data"
GOLDEN_OUTPUT_DIR = PROJECT_ROOT / "output/verification"

# Configuration Nx=4
Nx4_nk = 10  # Number of kernels for Nx=4
Nx4_kerX = 9  # Kernel size for Nx=4
Nx4_kerY = 9
Nx4_convX = 17  # Output size for Nx=4
Nx4_convY = 17

# Configuration Nx=16
Nx16_nk = 4  # Number of kernels for Nx=16
Nx16_kerX = 33  # Kernel size for Nx=16
Nx16_kerY = 33
Nx16_convX = 65  # Output size for Nx=16
Nx16_convY = 65


def load_binary_data(filepath):
    """Load binary float data"""
    data = np.fromfile(filepath, dtype=np.float32)
    return data


def check_hls_data():
    """Check HLS data directory (Nx=4 config)"""
    print("\n" + "=" * 60)
    print("HLS Data Directory Check (Nx=4 Configuration)")
    print("=" * 60)

    # Check input data
    print("\n[Input Data]")

    # Mask spectrum
    mskf_r = load_binary_data(HLS_DATA_DIR / "mskf_r.bin")
    mskf_i = load_binary_data(HLS_DATA_DIR / "mskf_i.bin")
    print(
        f"  mskf_r.bin: {mskf_r.shape}, range [{mskf_r.min():.4f}, {mskf_r.max():.4f}]"
    )
    print(
        f"  mskf_i.bin: {mskf_i.shape}, range [{mskf_i.min():.4f}, {mskf_i.max():.4f}]"
    )
    print(f"  Expected: 512×512 = {512*512}")

    # Eigenvalues
    scales = load_binary_data(HLS_DATA_DIR / "scales.bin")
    print(f"  scales.bin: {scales.shape}, values = {scales}")
    print(f"  Expected: {Nx4_nk} eigenvalues")

    # Kernels
    print("\n[Kernels]")
    for k in range(Nx4_nk):
        krn_r = load_binary_data(HLS_DATA_DIR / f"kernels/krn_{k}_r.bin")
        krn_i = load_binary_data(HLS_DATA_DIR / f"kernels/krn_{k}_i.bin")
        print(f"  Kernel {k}: {krn_r.shape} (expected {Nx4_kerX*Nx4_kerY})")
        print(f"    R range [{krn_r.min():.6f}, {krn_r.max():.6f}]")
        print(f"    I range [{krn_i.min():.6f}, {krn_i.max():.6f}]")

    # Golden output
    print("\n[Golden Output]")
    golden = load_binary_data(HLS_DATA_DIR / "tmpImgp_pad32.bin")
    print(f"  tmpImgp_pad32.bin: {golden.shape}")
    print(f"  Expected: {Nx4_convX*Nx4_convY} = {Nx4_convX*Nx4_convY}")
    print(f"  Range [{golden.min():.4f}, {golden.max():.4f}]")
    print(f"  Mean {golden.mean():.4f}")


def check_golden_output():
    """Check Golden output directory (Nx=16 config)"""
    print("\n" + "=" * 60)
    print("Golden Output Directory Check (Nx=16 Configuration)")
    print("=" * 60)

    # Check input data
    print("\n[Input Data]")

    # Mask spectrum (should be same as HLS data)
    mskf_r = load_binary_data(GOLDEN_OUTPUT_DIR / "mskf_r.bin")
    mskf_i = load_binary_data(GOLDEN_OUTPUT_DIR / "mskf_i.bin")
    print(
        f"  mskf_r.bin: {mskf_r.shape}, range [{mskf_r.min():.4f}, {mskf_r.max():.4f}]"
    )
    print(
        f"  mskf_i.bin: {mskf_i.shape}, range [{mskf_i.min():.4f}, {mskf_i.max():.4f}]"
    )

    # Eigenvalues
    scales = load_binary_data(GOLDEN_OUTPUT_DIR / "scales.bin")
    print(f"  scales.bin: {scales.shape}, values = {scales}")
    print(f"  Expected: {Nx16_nk} eigenvalues for Nx=16")

    # Kernels
    print("\n[Kernels]")
    for k in range(Nx16_nk):
        krn_r = load_binary_data(GOLDEN_OUTPUT_DIR / f"kernels/krn_{k}_r.bin")
        krn_i = load_binary_data(GOLDEN_OUTPUT_DIR / f"kernels/krn_{k}_i.bin")
        print(f"  Kernel {k}: {krn_r.shape} (expected {Nx16_kerX*Nx16_kerY})")
        print(f"    R range [{krn_r.min():.6f}, {krn_r.max():.6f}]")
        print(f"    I range [{krn_i.min():.6f}, {krn_i.max():.6f}]")

    # Output data
    print("\n[Output Data]")

    # tmpImgp variants
    tmpImgp_pad32 = load_binary_data(GOLDEN_OUTPUT_DIR / "tmpImgp_pad32.bin")
    tmpImgp_pad128 = load_binary_data(GOLDEN_OUTPUT_DIR / "tmpImgp_pad128.bin")
    print(f"  tmpImgp_pad32.bin: {tmpImgp_pad32.shape}")
    print(f"  tmpImgp_pad128.bin: {tmpImgp_pad128.shape}")

    # Full outputs
    aerial_tcc = load_binary_data(GOLDEN_OUTPUT_DIR / "aerial_image_tcc_direct.bin")
    aerial_socs = load_binary_data(GOLDEN_OUTPUT_DIR / "aerial_image_socs_kernel.bin")
    print(f"  aerial_image_tcc_direct.bin: {aerial_tcc.shape}")
    print(f"  aerial_image_socs_kernel.bin: {aerial_socs.shape}")


def compare_mask_spectrum():
    """Compare mask spectrum between HLS data and Golden output"""
    print("\n" + "=" * 60)
    print("Mask Spectrum Comparison")
    print("=" * 60)

    # Load both
    hls_mskf_r = load_binary_data(HLS_DATA_DIR / "mskf_r.bin")
    hls_mskf_i = load_binary_data(HLS_DATA_DIR / "mskf_i.bin")
    golden_mskf_r = load_binary_data(GOLDEN_OUTPUT_DIR / "mskf_r.bin")
    golden_mskf_i = load_binary_data(GOLDEN_OUTPUT_DIR / "mskf_i.bin")

    print(f"\nHLS mskf_r: {hls_mskf_r.shape}")
    print(f"Golden mskf_r: {golden_mskf_r.shape}")

    if hls_mskf_r.shape == golden_mskf_r.shape:
        diff_r = np.abs(hls_mskf_r - golden_mskf_r)
        diff_i = np.abs(hls_mskf_i - golden_mskf_i)
        print(f"  ✓ Same size")
        print(f"  Max diff R: {diff_r.max():.6e}")
        print(f"  Max diff I: {diff_i.max():.6e}")
        if diff_r.max() < 1e-6 and diff_i.max() < 1e-6:
            print(f"  ✓ Mask spectrum MATCHES")
        else:
            print(f"  ⚠ Mask spectrum differs")
    else:
        print(f"  ⚠ Different sizes!")


def analyze_configuration_difference():
    """Analyze why configurations differ"""
    print("\n" + "=" * 60)
    print("Configuration Difference Analysis")
    print("=" * 60)

    print("\n[Nx=4 Configuration (HLS Data)]")
    print(f"  Dynamic calculation: Nx = floor(NA * Lx * (1+σ) / λ)")
    print(f"  NA=0.8, Lx=512, λ=193, σ_outer=0.9")
    print(f"  Nx = floor(0.8 × 512 × 1.9 / 193) = floor(4.02) = 4")
    print(f"  Output size: 4×Nx+1 = {Nx4_convX}")
    print(f"  Kernel size: 2×Nx+1 = {Nx4_kerX}")
    print(f"  Number of kernels (nk): {Nx4_nk}")

    print("\n[Nx=16 Configuration (Golden Output)]")
    print(f"  Configuration file may have different parameters")
    print(f"  Output size: {Nx16_convX}")
    print(f"  Kernel size: {Nx16_kerX}")
    print(f"  Number of kernels (nk): {Nx16_nk}")

    print("\n[Recommendation]")
    print(f"  HLS data (Nx=4) is correct for golden_original.json")
    print(f"  Golden output (Nx=16) is from different config (config_Nx16_exact.json)")
    print(f"  For HLS validation, use HLS data directory files")


def main():
    print("=" * 60)
    print("Golden Data Consistency Check")
    print("FPGA-Litho SOCS Validation")
    print("=" * 60)

    # Check HLS data directory
    check_hls_data()

    # Check Golden output directory
    check_golden_output()

    # Compare mask spectrum
    compare_mask_spectrum()

    # Analyze configuration difference
    analyze_configuration_difference()

    print("\n" + "=" * 60)
    print("CHECK COMPLETE")
    print("=" * 60)


if __name__ == "__main__":
    main()
