#!/usr/bin/env python3
"""
Generate TCL commands for SOCS Optimized HLS IP board validation
Uses JTAG-to-AXI to write data to DDR and read results

Author: FPGA-Litho Project
Version: 1.0 (for Vivado FFT IP version)
"""

import struct
import json
import os
from pathlib import Path
from datetime import datetime

# =====================================================================
# Configuration
# =====================================================================

# Paths
WORKSPACE_ROOT = Path("e:/fpga-litho-accel")
GOLDEN_DIR = WORKSPACE_ROOT / "output" / "verification"
OUTPUT_DIR = (
    WORKSPACE_ROOT / "source" / "SOCS_HLS" / "board_validation" / "socs_optimized"
)
CONFIG_FILE = OUTPUT_DIR / "address_map.json"

# AXI burst parameters
MAX_BURST_LEN = 128  # JTAG-to-AXI maximum burst length
MAX_WORDS_PER_TCL = 64  # Reduce TCL file size

# =====================================================================
# Data Processing Functions
# =====================================================================


def read_bin_file(filepath):
    """Read BIN file as float32 array"""
    with open(filepath, "rb") as f:
        data = f.read()

    # Unpack as float32
    num_floats = len(data) // 4
    floats = struct.unpack(f"{num_floats}f", data)

    return floats, num_floats


def float_to_hex(f):
    """Convert float32 to 32-bit hex string"""
    packed = struct.pack("f", f)
    hex_val = struct.unpack("I", packed)[0]
    return f"0x{hex_val:08x}"


def hex_to_float(hex_str):
    """Convert hex string to float32"""
    hex_val = int(hex_str, 16)
    packed = struct.pack("I", hex_val)
    return struct.unpack("f", packed)[0]


# =====================================================================
# TCL Generation Functions
# =====================================================================


def generate_header():
    """Generate TCL script header"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    header = f"""#
# SOCS Optimized HLS IP Board Validation TCL Script
# Generated: {timestamp}
# Device: xcku3p-ffvb676-2-e
# Clock: 5ns (200MHz)
#
# This script:
# 1. Connects to hardware and programs device
# 2. Writes input data to DDR via JTAG-to-AXI
# 3. Starts HLS IP execution
# 4. Reads output results from DDR
# 5. Saves results for verification
#

# =====================================================================
# Step 1: Hardware Connection and Programming
# =====================================================================

puts "\\n=== SOCS Optimized HLS IP Board Validation ==="
puts "Connecting to hardware..."

# Connect to hardware server
connect_hw_server -url localhost:3121
open_hw_target

# Program FPGA
puts "Programming FPGA..."
program_hw_devices [get_hw_devices]

# Reset JTAG-to-AXI Master
puts "Resetting JTAG-to-AXI..."
reset_hw_axi [get_hw_axis hw_axi_1]

puts "Hardware ready.\\n"

"""
    return header


def generate_write_transaction(txn_id, address, hex_data_list, burst_len, comment=""):
    """Generate single TCL write transaction"""
    hex_str = " ".join(hex_data_list)

    # Clean txn_id to be valid TCL identifier (remove spaces, parentheses)
    clean_txn_id = str(txn_id).replace(" ", "_").replace("(", "").replace(")", "")

    tcl_cmd = f"""# Transaction {txn_id}: {burst_len} words @ 0x{address:08x} {comment}
create_hw_axi_txn write_txn_{clean_txn_id} [get_hw_axis hw_axi_1] \\
    -type WRITE \\
    -address 0x{address:08x} \\
    -data {{{hex_str}}} \\
    -len {burst_len}
run_hw_axi [get_hw_axi_txns write_txn_{clean_txn_id}]

"""
    return tcl_cmd


def generate_data_write_section(
    bin_file, ddr_addr, data_name, chunk_size=MAX_WORDS_PER_TCL
):
    """Generate TCL commands to write BIN file to DDR"""
    floats, total_words = read_bin_file(bin_file)

    section = f"""# =====================================================================
# Write {data_name} to DDR @ 0x{ddr_addr:08x}
# File: {bin_file.name}
# Total: {total_words} floats ({total_words*4} bytes)
# =====================================================================

puts "Writing {data_name} to DDR..."

"""

    txn_id = 0
    offset = 0

    while offset < total_words:
        chunk_len = min(chunk_size, total_words - offset)
        chunk_floats = floats[offset : offset + chunk_len]
        hex_data = [float_to_hex(f) for f in chunk_floats]
        block_addr = ddr_addr + offset * 4

        section += generate_write_transaction(
            f"{data_name}_{txn_id}",
            block_addr,
            hex_data,
            chunk_len,
            f"# {data_name} chunk {txn_id}",
        )

        offset += chunk_len
        txn_id += 1

    section += f'puts "{data_name} write complete.\\n"\n\n'
    return section


def generate_ip_control_section():
    """Generate TCL commands to control HLS IP"""
    section = """# =====================================================================
# HLS IP Control Sequence
# =====================================================================

puts "Starting HLS IP execution..."

# Step 1: Write ap_start (bit 0) to control register
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \\
    -type WRITE \\
    -address 0x00000000 \\
    -data {0x00000001} \\
    -len 1
run_hw_axi [get_hw_axi_txns start_txn]

puts "HLS IP started. Waiting for completion..."

# Step 2: Poll ap_done (bit 1 of control register)
# Note: In real implementation, add timeout protection
set max_polls 1000
set poll_count 0
set done 0

while {$poll_count < $max_polls && !$done} {
    # Read control register
    create_hw_axi_txn poll_txn [get_hw_axis hw_axi_1] \\
        -type READ \\
        -address 0x00000000 \\
        -len 1
    run_hw_axi [get_hw_axi_txns poll_txn]
    
    # Check ap_done bit (bit 1)
    set result [report_hw_axi_txn poll_txn]
    if {[string match "*0x00000002*" $result] || [string match "*0x00000003*" $result]} {
        set done 1
        puts "HLS IP completed after $poll_count polls."
    } else {
        incr poll_count
        after 10  # Wait 10ms between polls
    }
}

if {!$done} {
    puts "ERROR: HLS IP did not complete within timeout!"
    exit 1
}

puts "HLS IP execution complete.\\n"

"""
    return section


def generate_read_section(
    ddr_addr, output_name, num_words, chunk_size=MAX_WORDS_PER_TCL
):
    """Generate TCL commands to read output from DDR"""
    section = f"""# =====================================================================
# Read {output_name} from DDR @ 0x{ddr_addr:08x}
# Total: {num_words} floats ({num_words*4} bytes)
# =====================================================================

puts "Reading {output_name} from DDR..."

"""

    txn_id = 0
    offset = 0

    while offset < num_words:
        chunk_len = min(chunk_size, num_words - offset)
        block_addr = ddr_addr + offset * 4

        section += f"""# Read chunk {txn_id}: {chunk_len} words @ 0x{block_addr:08x}
create_hw_axi_txn read_{output_name}_{txn_id} [get_hw_axis hw_axi_1] \\
    -type READ \\
    -address 0x{block_addr:08x} \\
    -len {chunk_len}
run_hw_axi [get_hw_axi_txns read_{output_name}_{txn_id}]
report_hw_axi_txn read_{output_name}_{txn_id} > {OUTPUT_DIR}/{output_name}_chunk{txn_id}.txt

"""
        offset += chunk_len
        txn_id += 1

    section += f'puts "{output_name} read complete.\\n"\n\n'
    return section


def generate_footer():
    """Generate TCL script footer"""
    footer = (
        """# =====================================================================
# Validation Complete
# =====================================================================

puts "\\n=== Board Validation Complete ==="
puts "Results saved to: """
        + str(OUTPUT_DIR)
        + """"
puts "Run Python verification script to compare with Golden data."

# Disconnect from hardware
disconnect_hw_server

exit
"""
    )
    return footer


# =====================================================================
# Main Generation Function
# =====================================================================


def combine_kernel_files(kernel_dir, nk=4):
    """Combine individual kernel files into single arrays"""
    import numpy as np

    krn_r_all = []
    krn_i_all = []

    for k in range(nk):
        krn_r_file = kernel_dir / f"krn_{k}_r.bin"
        krn_i_file = kernel_dir / f"krn_{k}_i.bin"

        if krn_r_file.exists() and krn_i_file.exists():
            r_data, _ = read_bin_file(krn_r_file)
            i_data, _ = read_bin_file(krn_i_file)
            krn_r_all.extend(r_data)
            krn_i_all.extend(i_data)
            print(f"  Loaded kernel {k}: {len(r_data)} elements")
        else:
            print(f"  WARNING: Kernel {k} files not found")

    return krn_r_all, krn_i_all


def generate_full_tcl_script():
    """Generate complete TCL script for board validation"""

    # Load address map
    with open(CONFIG_FILE, "r") as f:
        config = json.load(f)

    ddr_segments = config["ddr_memory"]["segments"]

    # Start building script
    tcl_script = generate_header()

    # Write input data sections
    input_files = [
        ("gmem0", "mskf_r.bin", "Mask Spectrum (Real)"),
        ("gmem1", "mskf_i.bin", "Mask Spectrum (Imag)"),
        ("gmem2", "scales.bin", "Eigenvalues"),
    ]

    for key, filename, description in input_files:
        bin_file = GOLDEN_DIR / filename
        if bin_file.exists():
            ddr_addr = int(ddr_segments[key]["base"], 16)
            tcl_script += generate_data_write_section(bin_file, ddr_addr, description)
        else:
            print(f"WARNING: {bin_file} not found, skipping {description}")

    # Handle kernel files (combine individual kernel files)
    kernel_dir = GOLDEN_DIR / "kernels"
    if kernel_dir.exists():
        print("\nCombining kernel files...")
        krn_r_all, krn_i_all = combine_kernel_files(kernel_dir)

        # Write combined kernel data
        if krn_r_all:
            ddr_addr = int(ddr_segments["gmem3"]["base"], 16)
            # Create temporary combined file
            temp_r_file = OUTPUT_DIR / "krn_r_combined.bin"
            with open(temp_r_file, "wb") as f:
                for val in krn_r_all:
                    f.write(struct.pack("f", val))
            tcl_script += generate_data_write_section(
                temp_r_file, ddr_addr, "SOCS Kernels (Real)"
            )

        if krn_i_all:
            ddr_addr = int(ddr_segments["gmem4"]["base"], 16)
            # Create temporary combined file
            temp_i_file = OUTPUT_DIR / "krn_i_combined.bin"
            with open(temp_i_file, "wb") as f:
                for val in krn_i_all:
                    f.write(struct.pack("f", val))
            tcl_script += generate_data_write_section(
                temp_i_file, ddr_addr, "SOCS Kernels (Imag)"
            )
    else:
        print(f"WARNING: Kernel directory {kernel_dir} not found")

    # IP control section
    tcl_script += generate_ip_control_section()

    # Read output section
    output_addr = int(ddr_segments["gmem5"]["base"], 16)
    output_words = ddr_segments["gmem5"]["total_elements"]
    tcl_script += generate_read_section(output_addr, "output", output_words)

    # Footer
    tcl_script += generate_footer()

    return tcl_script


def main():
    """Main entry point"""
    print("=" * 60)
    print("SOCS Optimized HLS IP Board Validation TCL Generator")
    print("=" * 60)

    # Generate TCL script
    tcl_script = generate_full_tcl_script()

    # Write to file
    output_file = OUTPUT_DIR / "run_socs_optimized_validation.tcl"
    with open(output_file, "w") as f:
        f.write(tcl_script)

    print(f"\nGenerated TCL script: {output_file}")
    print(f"Total size: {len(tcl_script)} characters")

    # Generate summary
    print("\n" + "=" * 60)
    print("Generation Summary:")
    print("=" * 60)

    with open(CONFIG_FILE, "r") as f:
        config = json.load(f)

    ddr_segments = config["ddr_memory"]["segments"]

    print("\nInput Data to Write:")
    for key in ["gmem0", "gmem1", "gmem2", "gmem3", "gmem4"]:
        seg = ddr_segments[key]
        print(f"  {seg['name']:30} @ {seg['base']} ({seg['total_bytes']} bytes)")

    print(f"\nOutput to Read:")
    seg = ddr_segments["gmem5"]
    print(f"  {seg['name']:30} @ {seg['base']} ({seg['total_bytes']} bytes)")

    print("\n" + "=" * 60)
    print("Next Steps:")
    print("1. Open Vivado Hardware Manager")
    print("2. Connect to hardware (localhost:3121)")
    print("3. Source the generated TCL script:")
    print(f"   source {output_file}")
    print("4. Run Python verification script to compare results")
    print("=" * 60)


if __name__ == "__main__":
    main()
