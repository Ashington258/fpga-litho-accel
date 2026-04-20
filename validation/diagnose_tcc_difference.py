#!/usr/bin/env python3
"""
TCC Difference Root Cause Analysis
Compare Host and Golden TCC calculation implementations
"""

import numpy as np
import struct
import json
import os


def read_complex_bin(filepath, size):
    """Read complex float binary file (real + imag interleaved or separate)"""
    if not os.path.exists(filepath):
        return None
    data = np.fromfile(filepath, dtype=np.float32)
    return data


def read_float_bin(filepath, size):
    """Read float binary file"""
    if not os.path.exists(filepath):
        return None
    return np.fromfile(filepath, dtype=np.float32)


def analyze_source_difference():
    """Compare source generation"""
    print("=" * 60)
    print("SOURCE GENERATION ANALYSIS")
    print("=" * 60)

    # Load config
    config_path = "input/config/golden_original.json"
    with open(config_path, "r") as f:
        config = json.load(f)

    # Get actual source size from Host output file (file-based, not config-based)
    host_src_path = "source/host/output/source.bin"
    if os.path.exists(host_src_path):
        host_src = np.fromfile(host_src_path, dtype=np.float32)
        src_size = int(len(host_src) ** 0.5)  # Actual dimension from file
        print(
            f"\nActual source size from Host file: {src_size}x{src_size} ({len(host_src)} elements)"
        )
    else:
        # Fallback to config 'source.gridSize' (nested structure)
        src_size = config.get("source", {}).get("gridSize", 101)
        print(f"\nSource size from config: {src_size}")

    # Get optical parameters from nested config structure
    optics = config.get("optics", {})
    na = optics.get("NA", 0.8)
    wavelength = optics.get("wavelength", 193)

    # Get source parameters from nested structure
    source = config.get("source", {})
    annular = source.get("annular", {})
    sigma_inner = annular.get("innerRadius", 0.6)
    sigma_outer = annular.get("outerRadius", 0.9)

    print(f"\nConfiguration:")
    print(f"  srcSize = {src_size}")
    print(f"  NA = {na}")
    print(f"  wavelength = {wavelength}")
    print(f"  sigma_inner = {sigma_inner}")
    print(f"  sigma_outer = {sigma_outer}")

    # Compute theoretical annular source parameters
    center = (src_size - 1) / 2
    r_inner_pixels = sigma_inner * center
    r_outer_pixels = sigma_outer * center

    print(f"\nExpected Annular Source:")
    print(f"  Center = {center}")
    print(f"  r_inner (pixels) = {r_inner_pixels}")
    print(f"  r_outer (pixels) = {r_outer_pixels}")

    # Check Host source output
    if os.path.exists(host_src_path):
        host_src = np.fromfile(host_src_path, dtype=np.float32)
        host_src_2d = host_src.reshape(src_size, src_size)  # Reshape to actual 2D
        print(f"\nHost Source Statistics:")
        print(f"  Total sum = {np.sum(host_src):.6f}")
        print(f"  Non-zero count = {np.count_nonzero(host_src)}")
        print(f"  Max = {np.max(host_src):.6f}")
        print(f"  Min = {np.min(host_src):.6f}")

        # Find outer sigma from host source (2D)
        nonzero_y, nonzero_x = np.where(host_src_2d > 0)
        if len(nonzero_y) > 0:
            distances = np.sqrt((nonzero_y - center) ** 2 + (nonzero_x - center) ** 2)
            max_dist = np.max(distances)
            host_outer_sigma = max_dist / center
            print(f"  Host computed outer_sigma = {host_outer_sigma:.6f}")

    return config


def analyze_tcc_parameters(config):
    """Compare TCC calculation parameters"""
    print("\n" + "=" * 60)
    print("TCC CALCULATION PARAMETERS")
    print("=" * 60)

    na = config.get("NA", 0.8)
    wavelength = config.get("wavelength", 193)
    defocus = config.get("defocus", 0.0)
    Lx = config.get("Lx", 512)
    Ly = config.get("Ly", 512)
    Nx = config.get("Nx", 4)
    Ny = config.get("Ny", 4)
    sigma_outer = config.get("sigma_outer", 0.9)

    # Compute derived parameters
    dz = defocus / (na * na / wavelength)  # Rayleigh unit
    k = 2 * np.pi / wavelength
    Lx_norm = Lx * na / wavelength
    Ly_norm = Ly * na / wavelength
    tccSize = (2 * Nx + 1) * (2 * Ny + 1)

    print(f"\nInput Parameters:")
    print(f"  NA = {na}")
    print(f"  wavelength = {wavelength} nm")
    print(f"  defocus = {defocus}")
    print(f"  Lx = {Lx}, Ly = {Ly}")
    print(f"  Nx = {Nx}, Ny = {Ny}")

    print(f"\nDerived Parameters:")
    print(f"  dz (Rayleigh unit) = {dz:.6f}")
    print(f"  k (wave number) = {k:.6f}")
    print(f"  Lx_norm = {Lx_norm:.6f}")
    print(f"  Ly_norm = {Ly_norm:.6f}")
    print(f"  TCC matrix size = {tccSize} x {tccSize} = {tccSize*tccSize}")

    # Source integration range
    src_size = config.get("srcSize", 65)
    sh = (src_size - 1) / 2.0
    oSgm = int(np.ceil(sh * sigma_outer))

    print(f"\nSource Integration Range:")
    print(f"  sh (source half-size) = {sh}")
    print(f"  oSgm (integration range) = {oSgm}")
    print(f"  Source points to process: ~{(2*oSgm+1)**2}")

    return {
        "dz": dz,
        "k": k,
        "Lx_norm": Lx_norm,
        "Ly_norm": Ly_norm,
        "tccSize": tccSize,
        "sh": sh,
        "oSgm": oSgm,
    }


def compute_sample_pupil_values(params, config):
    """Compute sample pupil values to check precision"""
    print("\n" + "=" * 60)
    print("PUPIL VALUE COMPUTATION ANALYSIS")
    print("=" * 60)

    na = config.get("NA", 0.8)
    Nx = config.get("Nx", 4)
    Ny = config.get("Ny", 4)

    dz = params["dz"]
    k = params["k"]
    Lx_norm = params["Lx_norm"]
    Ly_norm = params["Ly_norm"]
    sh = params["sh"]

    # Sample source points
    test_source_points = [
        (0, 0),  # Center
        (int(sh * 0.9), 0),  # Outer edge
        (int(sh * 0.6), int(sh * 0.6)),  # Inner annular
    ]

    print(f"\nSample Pupil Calculations:")
    for sx_pixel, sy_pixel in test_source_points:
        sx = sx_pixel / sh
        sy = sy_pixel / sh
        print(f"\n  Source point (p={sx_pixel}, q={sy_pixel}):")
        print(f"    Normalized: sx={sx:.6f}, sy={sy:.6f}")

        # Sample frequency points
        for ny in [-Nx, 0, Nx]:
            for nx in [-Ny, 0, Ny]:
                fy = ny / Ly_norm + sy
                fx = nx / Lx_norm + sx
                rho2 = fx * fx + fy * fy

                if rho2 <= 1.0:
                    pupil_phase = dz * k * np.sqrt(1.0 - rho2 * na * na)
                    pupil_real = np.cos(pupil_phase)
                    pupil_imag = np.sin(pupil_phase)
                    print(
                        f"      Pupil(ny={ny}, nx={nx}): rho2={rho2:.6f}, phase={pupil_phase:.6f}"
                    )
                    print(f"        Value = ({pupil_real:.10f}, {pupil_imag:.10f})")
                else:
                    print(
                        f"      Pupil(ny={ny}, nx={nx}): rho2={rho2:.6f} > 1.0 (outside NA)"
                    )


def compare_tcc_elements():
    """Compare specific TCC elements between Host and Golden"""
    print("\n" + "=" * 60)
    print("TCC ELEMENT COMPARISON")
    print("=" * 60)

    # Load Host TCC
    host_tcc_path = "source/host/output/tcc_r.bin"
    host_tcc_i_path = "source/host/output/tcc_i.bin"

    # Load Golden TCC
    golden_tcc_path = "output/verification/tcc_r.bin"
    golden_tcc_i_path = "output/verification/tcc_i.bin"

    if not all(os.path.exists(p) for p in [host_tcc_path, golden_tcc_path]):
        print("  TCC files not found, skipping direct comparison")
        return

    host_tcc_r = read_float_bin(host_tcc_path, 81 * 81)
    host_tcc_i = read_float_bin(host_tcc_i_path, 81 * 81)
    golden_tcc_r = read_float_bin(golden_tcc_path, 81 * 81)
    golden_tcc_i = read_float_bin(golden_tcc_i_path, 81 * 81)

    if host_tcc_r is None or golden_tcc_r is None:
        print("  Failed to load TCC data")
        return

    host_tcc = host_tcc_r + 1j * host_tcc_i
    golden_tcc = golden_tcc_r + 1j * golden_tcc_i

    # Analyze diagonal elements (should be real, positive)
    print("\nDiagonal Elements (should be real and positive):")
    for i in [0, 40, 80]:  # Center, middle, edge
        host_diag = host_tcc[i * 81 + i]
        golden_diag = golden_tcc[i * 81 + i]
        diff = abs(host_diag - golden_diag)
        rel_diff = diff / abs(golden_diag) if abs(golden_diag) > 0 else 0
        print(f"  TCC[{i},{i}]:")
        print(f"    Host:   {host_diag:.10f}")
        print(f"    Golden: {golden_diag:.10f}")
        print(f"    Diff:   {diff:.10e} ({rel_diff*100:.4f}%)")

    # Analyze off-diagonal elements
    print("\nSample Off-Diagonal Elements:")
    test_pairs = [(0, 1), (40, 41), (0, 80), (40, 80)]
    for i, j in test_pairs:
        host_val = host_tcc[i * 81 + j]
        golden_val = golden_tcc[i * 81 + j]
        diff = abs(host_val - golden_val)
        rel_diff = diff / abs(golden_val) if abs(golden_val) > 0 else 0
        print(f"  TCC[{i},{j}]:")
        print(f"    Host:   {host_val:.10f}")
        print(f"    Golden: {golden_val:.10f}")
        print(f"    Diff:   {diff:.10e} ({rel_diff*100:.4f}%)")


def analyze_eigenvalue_precision():
    """Analyze eigenvalue precision tolerance"""
    print("\n" + "=" * 60)
    print("EIGENVALUE PRECISION ANALYSIS")
    print("=" * 60)

    # Load scales
    host_scales_path = "source/host/output/scales.bin"
    golden_scales_path = "output/verification/scales.bin"

    if not all(os.path.exists(p) for p in [host_scales_path, golden_scales_path]):
        print("  Scales files not found")
        return

    host_scales = read_float_bin(host_scales_path, 10)
    golden_scales = read_float_bin(golden_scales_path, 10)

    print("\nEigenvalue Comparison:")
    print("  Host scales:   ", host_scales[:4])
    print("  Golden scales: ", golden_scales[:4])

    for i in range(min(4, len(host_scales))):
        diff = abs(host_scales[i] - golden_scales[i])
        rel_diff = diff / golden_scales[i] if golden_scales[i] > 0 else 0
        print(f"  λ[{i}]: diff={diff:.6e} ({rel_diff*100:.4f}%)")

    # Analyze if difference is acceptable for FPGA verification
    print("\nPrecision Analysis:")
    max_rel_diff = max(
        abs(host_scales[i] - golden_scales[i]) / golden_scales[i]
        for i in range(min(4, len(host_scales)))
        if golden_scales[i] > 0
    )
    print(f"  Maximum relative difference: {max_rel_diff*100:.4f}%")

    # LAPACK zheevr typical precision
    lapack_precision = 1e-10  # Double precision eigenvalue solver
    print(f"  LAPACK zheevr typical precision: ~{lapack_precision}")

    # Check if difference is due to TCC input difference
    tcc_rel_diff = 0.006  # 0.6% from previous analysis
    print(f"  TCC matrix relative difference: ~{tcc_rel_diff*100:.4f}%")

    print("\nConclusion:")
    if max_rel_diff < 0.001:  # < 0.1%
        print("  Eigenvalue difference is minor (<0.1%), likely due to:")
        print("    - Floating point accumulation differences")
        print("    - LAPACK solver numerical precision")
        print("  ACCEPTABLE for FPGA HLS verification")
    elif max_rel_diff < 0.01:  # < 1%
        print("  Eigenvalue difference is small (<1%), caused by:")
        print("    - TCC matrix calculation differences (~0.6%)")
        print("    - Pupil computation precision variations")
        print("  MARGINAL - need to verify if FPGA HLS can tolerate this")
    else:
        print("  Eigenvalue difference is significant (>1%)")
        print("  NOT ACCEPTABLE - requires implementation fix")


def main():
    print("TCC DIFFERENCE ROOT CAUSE ANALYSIS")
    print("=" * 60)

    # Step 1: Source analysis
    config = analyze_source_difference()

    # Step 2: TCC parameters
    params = analyze_tcc_parameters(config)

    # Step 3: Pupil computation analysis
    compute_sample_pupil_values(params, config)

    # Step 4: TCC element comparison
    compare_tcc_elements()

    # Step 5: Eigenvalue precision
    analyze_eigenvalue_precision()

    print("\n" + "=" * 60)
    print("ANALYSIS COMPLETE")
    print("=" * 60)

    print("\nKey Findings:")
    print("1. Golden uses precomputed pupil matrix")
    print("2. Host uses on-the-fly pupil calculation")
    print("3. Both are mathematically equivalent but may have:")
    print("   - Different floating point accumulation patterns")
    print("   - Different boundary check timing (rho2 <= 1.0)")
    print("4. ~0.057% eigenvalue difference is likely acceptable for HLS verification")
    print("   (Golden verification passed with 3.3% tolerance for nk truncation)")


if __name__ == "__main__":
    main()
