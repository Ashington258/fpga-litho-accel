#!/usr/bin/env python3
"""
Quick check TCL output files format
"""

import struct
from pathlib import Path

# Configuration
RESULTS_DIR = Path(
    "e:/fpga-litho-accel/source/SOCS_HLS/board_validation/socs_optimized"
)


def hex_to_float(hex_str):
    """Convert hex string to float32"""
    hex_val = int(hex_str, 16)
    packed = struct.pack("I", hex_val)
    return struct.unpack("f", packed)[0]


def check_tcl_output_file(file_path):
    """Check TCL output file format"""
    print(f"\nChecking: {file_path.name}")

    if not file_path.exists():
        print(f"  ERROR: File not found")
        return False

    with open(file_path, "r") as f:
        content = f.read()

    # Parse TCL report format
    lines = content.strip().split("\n")
    print(f"  Lines: {len(lines)}")

    data_values = []
    for i, line in enumerate(lines[:5]):  # Show first 5 lines
        print(f"  Line {i}: {line[:100]}...")

        if "Data:" in line:
            hex_values = line.split("Data:")[1].strip().split()
            print(f"    Found {len(hex_values)} hex values")

            for hex_val in hex_values[:3]:  # Show first 3 values
                if hex_val.startswith("0x"):
                    float_val = hex_to_float(hex_val)
                    print(f"      {hex_val} -> {float_val:.6f}")

    return True


def main():
    """Main function"""
    print("=" * 60)
    print("Quick TCL Output Check")
    print("=" * 60)

    # Check for output files
    output_files = list(RESULTS_DIR.glob("output_chunk*.txt"))

    if not output_files:
        print("No output files found. Run TCL validation first.")
        return

    print(f"Found {len(output_files)} output files")

    # Check first file
    check_tcl_output_file(output_files[0])

    # Check if we have enough chunks for 289 elements (17x17)
    chunk_size = 64
    total_elements = 289
    expected_chunks = (total_elements + chunk_size - 1) // chunk_size

    print(f"\nExpected chunks for {total_elements} elements: {expected_chunks}")
    print(f"Actual chunks found: {len(output_files)}")

    if len(output_files) >= expected_chunks:
        print("✓ Sufficient output files found")
    else:
        print("✗ Insufficient output files")


if __name__ == "__main__":
    main()
