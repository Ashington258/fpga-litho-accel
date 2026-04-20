#!/usr/bin/env python3
"""
Compare Host intermediate data (source, TCC) with Golden reference
"""
import numpy as np
import os
import sys


def load_bin_file(filepath, dtype=np.float32):
    """Load binary file as numpy array"""
    if not os.path.exists(filepath):
        return None
    return np.fromfile(filepath, dtype=dtype)


def compare_arrays(host_arr, golden_arr, name, rtol=1e-5, atol=1e-8):
    """Compare two arrays and report statistics"""
    if host_arr is None or golden_arr is None:
        print(
            f"[{name}] Missing data - Host: {host_arr is not None}, Golden: {golden_arr is not None}"
        )
        return False

    if host_arr.shape != golden_arr.shape:
        print(
            f"[{name}] Shape mismatch: Host={host_arr.shape}, Golden={golden_arr.shape}"
        )
        return False

    # Basic statistics
    print(f"\n[{name}] Size: {host_arr.shape}")
    print(
        f"  Host: min={host_arr.min():.6e}, max={host_arr.max():.6e}, mean={host_arr.mean():.6e}"
    )
    print(
        f"  Golden: min={golden_arr.min():.6e}, max={golden_arr.max():.6e}, mean={golden_arr.mean():.6e}"
    )

    # Compute differences
    diff = host_arr - golden_arr
    abs_diff = np.abs(diff)
    rel_diff = abs_diff / (np.abs(golden_arr) + 1e-10)

    print(f"  Abs diff: max={abs_diff.max():.6e}, mean={abs_diff.mean():.6e}")
    print(f"  Rel diff: max={rel_diff.max():.6e}, mean={rel_diff.mean():.6e}")

    # Check if close
    is_close = np.allclose(host_arr, golden_arr, rtol=rtol, atol=atol)
    print(f"  np.allclose(rtol={rtol}, atol={atol}): {is_close}")

    # Find worst mismatches
    if abs_diff.max() > atol:
        worst_idx_flat = np.argmax(abs_diff.flatten())
        if len(host_arr.shape) == 1:
            print(
                f"  Worst mismatch at idx={worst_idx_flat}: host={host_arr[worst_idx_flat]:.6e}, golden={golden_arr[worst_idx_flat]:.6e}"
            )
        else:
            worst_idx_2d = np.unravel_index(worst_idx_flat, host_arr.shape)
            print(
                f"  Worst mismatch at [{worst_idx_2d[0]},{worst_idx_2d[1]}]: host={host_arr[worst_idx_2d]:.6e}, golden={golden_arr[worst_idx_2d]:.6e}"
            )

    return is_close


def main():
    # Paths
    host_dir = "source/host/output"
    golden_dir = "output/verification"

    print("=" * 60)
    print("Host vs Golden Intermediate Data Comparison")
    print("=" * 60)

    # Compare source matrices (Golden doesn't have source.bin, skip)
    print("\n--- Source Matrix Comparison ---")
    print("  Golden does not have source.bin, skipping source comparison")
    print("  Host source.bin saved for manual inspection if needed")

    # Compare TCC matrices (real part)
    print("\n--- TCC Real Part Comparison ---")
    host_tcc_r = load_bin_file(os.path.join(host_dir, "tcc_r.bin"))
    golden_tcc_r = load_bin_file(os.path.join(golden_dir, "tcc_r.bin"))

    if host_tcc_r is not None and golden_tcc_r is not None:
        # TCC is 81x81 = 6561
        tcc_size = 81
        host_tcc_r = host_tcc_r.reshape(tcc_size, tcc_size)
        golden_tcc_r = golden_tcc_r.reshape(tcc_size, tcc_size)
        compare_arrays(host_tcc_r, golden_tcc_r, "TCC Real", rtol=1e-4, atol=1e-6)

    # Compare TCC matrices (imag part)
    print("\n--- TCC Imag Part Comparison ---")
    host_tcc_i = load_bin_file(os.path.join(host_dir, "tcc_i.bin"))
    golden_tcc_i = load_bin_file(os.path.join(golden_dir, "tcc_i.bin"))

    if host_tcc_i is not None and golden_tcc_i is not None:
        host_tcc_i = host_tcc_i.reshape(tcc_size, tcc_size)
        golden_tcc_i = golden_tcc_i.reshape(tcc_size, tcc_size)
        compare_arrays(host_tcc_i, golden_tcc_i, "TCC Imag", rtol=1e-4, atol=1e-6)

    # Compare eigenvalues (scales)
    print("\n--- Scales (Eigenvalues) Comparison ---")
    host_scales = load_bin_file(os.path.join(host_dir, "scales.bin"))
    golden_scales = load_bin_file(os.path.join(golden_dir, "scales.bin"))

    if host_scales is not None and golden_scales is not None:
        compare_arrays(host_scales, golden_scales, "Scales", rtol=1e-4, atol=1e-6)

        # Print sorted eigenvalues for comparison
        print("\n  Sorted eigenvalues:")
        print(f"    Host (sorted): {np.sort(host_scales)[::-1]}")
        print(f"    Golden (sorted): {np.sort(golden_scales)[::-1]}")


if __name__ == "__main__":
    main()
