#!/usr/bin/env python3
"""
Verify SOCS Optimized HLS IP board validation results
Compare HLS output with Golden data

Author: FPGA-Litho Project
Version: 1.0
"""

import struct
import json
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

# =====================================================================
# Configuration
# =====================================================================

WORKSPACE_ROOT = Path("e:/fpga-litho-accel")
GOLDEN_DIR = WORKSPACE_ROOT / "output" / "verification"
RESULTS_DIR = (
    WORKSPACE_ROOT / "source" / "SOCS_HLS" / "board_validation" / "socs_optimized"
)
CONFIG_FILE = RESULTS_DIR / "address_map.json"

# =====================================================================
# Data Loading Functions
# =====================================================================


def read_bin_file(filepath):
    """Read BIN file as float32 array"""
    with open(filepath, "rb") as f:
        data = f.read()

    # Unpack as float32
    num_floats = len(data) // 4
    floats = struct.unpack(f"{num_floats}f", data)

    return np.array(floats, dtype=np.float32)


def load_tcl_output_files(output_dir, prefix, num_chunks):
    """Load TCL output files and combine into single array"""
    all_data = []

    for i in range(num_chunks):
        chunk_file = output_dir / f"{prefix}_chunk{i}.txt"
        if chunk_file.exists():
            # Read TCL report file
            with open(chunk_file, "r") as f:
                content = f.read()

            # Parse hex data from TCL report
            # Format: "Address: 0x44840000 Data: 0x3f800000 0x40000000 ..."
            lines = content.strip().split("\n")
            for line in lines:
                if "Data:" in line:
                    hex_values = line.split("Data:")[1].strip().split()
                    for hex_val in hex_values:
                        if hex_val.startswith("0x"):
                            # Convert hex to float
                            hex_int = int(hex_val, 16)
                            packed = struct.pack("I", hex_int)
                            float_val = struct.unpack("f", packed)[0]
                            all_data.append(float_val)

    return np.array(all_data, dtype=np.float32)


def load_golden_output():
    """Load golden output data"""
    golden_file = GOLDEN_DIR / "aerial_image_socs_kernel.bin"
    if golden_file.exists():
        golden_data = read_bin_file(golden_file)
        print(f"Loaded golden output: {len(golden_data)} elements")
        return golden_data
    else:
        print(f"WARNING: Golden file not found: {golden_file}")
        return None


# =====================================================================
# Verification Functions
# =====================================================================


def calculate_rmse(predicted, target):
    """Calculate Root Mean Square Error"""
    if len(predicted) != len(target):
        raise ValueError(f"Array length mismatch: {len(predicted)} vs {len(target)}")

    diff = predicted - target
    mse = np.mean(diff**2)
    rmse = np.sqrt(mse)

    return rmse


def calculate_relative_error(predicted, target):
    """Calculate relative error"""
    if len(predicted) != len(target):
        raise ValueError(f"Array length mismatch: {len(predicted)} vs {len(target)}")

    # Avoid division by zero
    mask = np.abs(target) > 1e-10
    if np.sum(mask) == 0:
        return np.inf

    relative_error = np.abs(predicted[mask] - target[mask]) / np.abs(target[mask])
    mean_relative_error = np.mean(relative_error)

    return mean_relative_error


def verify_results(hls_output, golden_output, output_shape=(17, 17)):
    """Verify HLS output against golden data"""

    print("\n" + "=" * 60)
    print("SOCS Optimized HLS IP Verification Results")
    print("=" * 60)

    # Reshape for visualization
    hls_2d = hls_output.reshape(output_shape)
    golden_2d = golden_output.reshape(output_shape)

    # Calculate metrics
    rmse = calculate_rmse(hls_output, golden_output)
    max_diff = np.max(np.abs(hls_output - golden_output))
    mean_diff = np.mean(np.abs(hls_output - golden_output))
    relative_error = calculate_relative_error(hls_output, golden_output)

    # Calculate correlation coefficient
    correlation = np.corrcoef(hls_output, golden_output)[0, 1]

    # Print results
    print(f"\nOutput Shape: {output_shape}")
    print(f"Total Elements: {len(hls_output)}")

    print(f"\nError Metrics:")
    print(f"  RMSE: {rmse:.6e}")
    print(f"  Max Absolute Difference: {max_diff:.6e}")
    print(f"  Mean Absolute Difference: {mean_diff:.6e}")
    print(f"  Mean Relative Error: {relative_error:.6e}")
    print(f"  Correlation Coefficient: {correlation:.6f}")

    # Check if within tolerance
    tolerance = 1e-3  # 0.1% tolerance
    if rmse < tolerance:
        print(f"\n✓ PASS - RMSE ({rmse:.6e}) < tolerance ({tolerance})")
        return True
    else:
        print(f"\n✗ FAIL - RMSE ({rmse:.6e}) >= tolerance ({tolerance})")
        return False


def create_comparison_plots(hls_output, golden_output, output_shape=(17, 17)):
    """Create comparison plots"""

    # Reshape for visualization
    hls_2d = hls_output.reshape(output_shape)
    golden_2d = golden_output.reshape(output_shape)
    diff_2d = hls_2d - golden_2d

    # Create figure
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))

    # Plot HLS output
    im1 = axes[0, 0].imshow(hls_2d, cmap="viridis", aspect="auto")
    axes[0, 0].set_title("HLS Output")
    plt.colorbar(im1, ax=axes[0, 0])

    # Plot Golden output
    im2 = axes[0, 1].imshow(golden_2d, cmap="viridis", aspect="auto")
    axes[0, 1].set_title("Golden Output")
    plt.colorbar(im2, ax=axes[0, 1])

    # Plot Difference
    im3 = axes[0, 2].imshow(diff_2d, cmap="RdBu", aspect="auto")
    axes[0, 2].set_title("Difference (HLS - Golden)")
    plt.colorbar(im3, ax=axes[0, 2])

    # Plot line comparison (center row)
    center_row = output_shape[0] // 2
    axes[1, 0].plot(hls_2d[center_row, :], "b-", label="HLS", linewidth=2)
    axes[1, 0].plot(golden_2d[center_row, :], "r--", label="Golden", linewidth=2)
    axes[1, 0].set_title(f"Center Row Comparison (row {center_row})")
    axes[1, 0].set_xlabel("Column")
    axes[1, 0].set_ylabel("Intensity")
    axes[1, 0].legend()
    axes[1, 0].grid(True)

    # Plot line comparison (center column)
    center_col = output_shape[1] // 2
    axes[1, 1].plot(hls_2d[:, center_col], "b-", label="HLS", linewidth=2)
    axes[1, 1].plot(golden_2d[:, center_col], "r--", label="Golden", linewidth=2)
    axes[1, 1].set_title(f"Center Column Comparison (col {center_col})")
    axes[1, 1].set_xlabel("Row")
    axes[1, 1].set_ylabel("Intensity")
    axes[1, 1].legend()
    axes[1, 1].grid(True)

    # Plot scatter comparison
    axes[1, 2].scatter(golden_output, hls_output, alpha=0.5, s=10)
    axes[1, 2].plot(
        [golden_output.min(), golden_output.max()],
        [golden_output.min(), golden_output.max()],
        "r--",
        linewidth=2,
    )
    axes[1, 2].set_title("Scatter Comparison")
    axes[1, 2].set_xlabel("Golden Output")
    axes[1, 2].set_ylabel("HLS Output")
    axes[1, 2].grid(True)

    plt.tight_layout()

    # Save figure
    output_file = RESULTS_DIR / "socs_optimized_verification.png"
    plt.savefig(output_file, dpi=150, bbox_inches="tight")
    print(f"\nSaved comparison plot: {output_file}")

    # Show plot
    plt.show()


# =====================================================================
# Main Verification Function
# =====================================================================


def main():
    """Main verification entry point"""
    print("=" * 60)
    print("SOCS Optimized HLS IP Results Verification")
    print("=" * 60)

    # Load configuration
    with open(CONFIG_FILE, "r") as f:
        config = json.load(f)

    output_config = config["ddr_memory"]["segments"]["gmem5"]
    output_shape = tuple(output_config["dimensions"])
    output_elements = output_config["total_elements"]

    print(f"\nExpected output shape: {output_shape}")
    print(f"Expected elements: {output_elements}")

    # Load HLS output from TCL results
    print("\nLoading HLS output from TCL results...")

    # Determine number of chunks (based on MAX_WORDS_PER_TCL=64)
    chunk_size = 64
    num_chunks = (output_elements + chunk_size - 1) // chunk_size

    hls_output = load_tcl_output_files(RESULTS_DIR, "output", num_chunks)

    if len(hls_output) == 0:
        print("ERROR: No HLS output data found!")
        print("Please run the TCL validation script first.")
        return False

    print(f"Loaded HLS output: {len(hls_output)} elements")

    # Load golden output
    print("\nLoading golden output...")
    golden_output = load_golden_output()

    if golden_output is None:
        print("ERROR: Golden output not found!")
        return False

    # Verify output size
    if len(hls_output) != len(golden_output):
        print(f"WARNING: Output size mismatch!")
        print(f"  HLS: {len(hls_output)} elements")
        print(f"  Golden: {len(golden_output)} elements")

        # Try to extract center region if sizes differ
        if len(golden_output) > len(hls_output):
            print("Extracting center region from golden output...")
            # Assuming golden is larger, extract center
            golden_2d = golden_output.reshape(512, 512)  # Assuming 512x512
            center_start = (512 - output_shape[0]) // 2
            center_end = center_start + output_shape[0]
            golden_center = golden_2d[center_start:center_end, center_start:center_end]
            golden_output = golden_center.flatten()
            print(f"Extracted center: {len(golden_output)} elements")

    # Verify results
    passed = verify_results(hls_output, golden_output, output_shape)

    # Create comparison plots
    print("\nCreating comparison plots...")
    try:
        create_comparison_plots(hls_output, golden_output, output_shape)
    except Exception as e:
        print(f"WARNING: Could not create plots: {e}")

    # Save results to file
    results_file = RESULTS_DIR / "verification_results.txt"
    with open(results_file, "w") as f:
        f.write("SOCS Optimized HLS IP Verification Results\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Output Shape: {output_shape}\n")
        f.write(f"Total Elements: {len(hls_output)}\n\n")

        rmse = calculate_rmse(hls_output, golden_output)
        max_diff = np.max(np.abs(hls_output - golden_output))
        mean_diff = np.mean(np.abs(hls_output - golden_output))
        relative_error = calculate_relative_error(hls_output, golden_output)
        correlation = np.corrcoef(hls_output, golden_output)[0, 1]

        f.write("Error Metrics:\n")
        f.write(f"  RMSE: {rmse:.6e}\n")
        f.write(f"  Max Absolute Difference: {max_diff:.6e}\n")
        f.write(f"  Mean Absolute Difference: {mean_diff:.6e}\n")
        f.write(f"  Mean Relative Error: {relative_error:.6e}\n")
        f.write(f"  Correlation Coefficient: {correlation:.6f}\n\n")

        tolerance = 1e-3
        if rmse < tolerance:
            f.write(f"✓ PASS - RMSE ({rmse:.6e}) < tolerance ({tolerance})\n")
        else:
            f.write(f"✗ FAIL - RMSE ({rmse:.6e}) >= tolerance ({tolerance})\n")

    print(f"\nSaved results to: {results_file}")

    print("\n" + "=" * 60)
    if passed:
        print("✓ VERIFICATION PASSED")
    else:
        print("✗ VERIFICATION FAILED")
    print("=" * 60)

    return passed


if __name__ == "__main__":
    main()
