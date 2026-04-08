#!/usr/bin/env python3
"""
Generate TCL commands for JTAG-to-AXI DDR write operations
Based on Vivado AddressSegments.csv mapping
Converts BIN files to Vivado TCL command sequences

Author: FPGA-Litho Project
Version: 1.0
"""

import struct
import os
import csv
from pathlib import Path

# =====================================================================
# Configuration Section
# =====================================================================

# Paths
GOLDEN_DIR = Path('/root/project/FPGA-Litho/output/verification')
OUTPUT_DIR = Path('/root/project/FPGA-Litho/source/SOCS_HLS/board_validation/scripts')
CSV_FILE = Path('/root/project/FPGA-Litho/source/SOCS_HLS/socs_full_csynth_v13/hls/impl/ip/AddressSegments.csv')

# AXI burst parameters
MAX_BURST_LEN = 256  # Vivado JTAG-to-AXI maximum burst length (256 beats)
MAX_WORDS_PER_TCL = 64  # Reduce TCL file size, split large bursts into smaller chunks

# =====================================================================
# Address Mapping Parser
# =====================================================================

def parse_address_segments_csv(csv_file):
    """
    Parse Vivado AddressSegments.csv to extract JTAG-to-AXI accessible addresses
    
    Returns dict:
        {
            'hls_control': 0x04000000,
            'hls_control_r': 0x04010000,
            'bram_mem': 0xC0000000
        }
    """
    addresses = {}
    
    with open(csv_file, 'r') as f:
        lines = f.readlines()
    
    # Find JTAG AXI Master address space
    for line in lines:
        if '/jtag_axi_0/Data' in line and not line.startswith('#'):
            parts = line.strip().split(',')
            if len(parts) >= 5:
                segment_name = parts[0]
                slave_segment = parts[2]
                offset_hex = parts[3]
                range_str = parts[4]
                
                # Extract offset address
                offset = int(offset_hex, 16)
                
                # Identify segment type
                if 's_axi_control/Reg' in slave_segment and 'Reg_1' not in segment_name:
                    addresses['hls_control'] = offset
                    print(f"[INFO] HLS AXI-Lite Control: 0x{offset:08X} ({range_str})")
                elif 's_axi_control_r/Reg' in slave_segment or 'Reg_1' in segment_name:
                    addresses['hls_control_r'] = offset
                    print(f"[INFO] HLS AXI-Lite Address: 0x{offset:08X} ({range_str})")
                elif 'axi_bram_ctrl_0' in slave_segment:
                    addresses['bram_mem'] = offset
                    print(f"[INFO] BRAM Memory: 0x{offset:08X} ({range_str})")
    
    # Validate addresses
    if not addresses:
        raise ValueError("No address segments found in CSV")
    
    required_keys = ['hls_control', 'hls_control_r', 'bram_mem']
    for key in required_keys:
        if key not in addresses:
            raise ValueError(f"Missing required address segment: {key}")
    
    return addresses

# =====================================================================
# Data Processing Functions
# =====================================================================

def read_bin_file(filepath):
    """
    Read BIN file as float32 array
    
    Args:
        filepath: Path to BIN file
    
    Returns:
        numpy-style list of float32 values
    """
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Unpack as float32
    num_floats = len(data) // 4
    floats = struct.unpack(f'{num_floats}f', data)
    
    return floats, num_floats

def float_to_hex(f):
    """
    Convert float32 to 32-bit hex string (for TCL)
    
    Args:
        f: float value
    
    Returns:
        hex string with 0x prefix (e.g., '0x3f800000')
    """
    # Pack as float32, unpack as uint32
    packed = struct.pack('f', f)
    hex_val = struct.unpack('I', packed)[0]
    return f'0x{hex_val:08x}'

def generate_write_transaction(txn_id, address, hex_data_list, burst_len):
    """
    Generate single TCL write transaction
    
    Args:
        txn_id: Transaction identifier
        address: DDR address (int)
        hex_data_list: List of hex strings
        burst_len: Number of 32-bit words
    
    Returns:
        TCL command string
    """
    # Join hex data with spaces and wrap in TCL braces
    hex_str = ' '.join(hex_data_list)
    
    # Vivado requires -type immediately after hw_axi object
    # -data must be enclosed in braces { }
    # CORRECTED: Use run_hw_axi + get_hw_axi_txns (NOT run_hw_axi_txn)
    tcl_cmd = f'''# Transaction {txn_id}: {burst_len} words @ 0x{address:08x}
create_hw_axi_txn write_txn_{txn_id} [get_hw_axis hw_axi_1] \
    -type WRITE \
    -address 0x{address:08x} \
    -data {{{hex_str}}} \
    -len {burst_len}
run_hw_axi [get_hw_axi_txns write_txn_{txn_id}]
'''
    return tcl_cmd

def generate_write_transactions_for_file(bin_file, ddr_addr, chunk_size=MAX_WORDS_PER_TCL):
    """
    Generate all TCL write transactions for a BIN file
    
    Args:
        bin_file: Path to BIN file
        ddr_addr: DDR base address for this data
        chunk_size: Words per transaction (reduce TCL file size)
    ic
    Returns:
        List of TCL command strings, total word count
    """
    floats, total_words = read_bin_file(bin_file)
    
    tcl_commands = []
    txn_id = 0
    offset = 0
    
    print(f"[INFO] Processing {bin_file.name}: {total_words} floats ({total_words*4} bytes)")
    
    while offset < total_words:
        # Calculate chunk length
        chunk_len = min(chunk_size, total_words - offset)
        
        # Extract chunk data
        chunk_floats = floats[offset:offset + chunk_len]
        
        # Convert to hex
        hex_data = [float_to_hex(f) for f in chunk_floats]
        
        # Calculate address
        block_addr = ddr_addr + offset * 4
        
        # Generate TCL command
        tcl_cmd = generate_write_transaction(txn_id, block_addr, hex_data, chunk_len)
        tcl_commands.append(tcl_cmd)
        
        offset += chunk_len
        txn_id += 1
    
    return tcl_commands, total_words, txn_id

# =====================================================================
# DDR Address Allocation
# =====================================================================

def allocate_ddr_addresses(bram_base):
    """
    Allocate DDR addresses for input/output buffers
    
    Args:
        bram_base: BRAM base address from CSV
    
    Returns:
        Dict mapping data type to DDR address
    """
    # BRAM size is 1M (0x100000 bytes)
    # Allocate within BRAM range (0xC0000000 - 0xC1000000)
    
    allocation = {
        'mskf_r': bram_base + 0x000000,    # 1MB (512×512×4)
        'mskf_i': bram_base + 0x100000,    # 1MB
        'scales': bram_base + 0x200000,    # 40B (10×4)
        'krn_r':  bram_base + 0x200100,    # 3240B (10×9×9×4)
        'krn_i':  bram_base + 0x201000,    # 3240B
        'output': bram_base + 0x202000,    # 1156B (289×4)
    }
    
    print("\n[INFO] DDR Address Allocation:")
    for name, addr in allocation.items():
        print(f"  {name}: 0x{addr:08X}")
    
    return allocation

# =====================================================================
# Complete TCL Script Generator
# =====================================================================

def generate_full_tcl_script(addresses, output_file):
    """
    Generate complete board validation TCL script
    
    Args:
        addresses: Dict from parse_address_segments_csv()
        output_file: Path to output TCL file
    """
    
    # Extract addresses
    hls_ctrl = addresses['hls_control']
    hls_ctrl_r = addresses['hls_control_r']
    bram_base = addresses['bram_mem']
    
    # DDR address allocation
    ddr_addrs = allocate_ddr_addresses(bram_base)
    
    # Header section
    tcl_content = '''# =====================================================================
# FPGA-Litho SOCS HLS Board Validation TCL Script
# Auto-generated by generate_jtag_tcl_from_csv.py
# =====================================================================
# Address Mapping (from Vivado AddressSegments.csv):
#   HLS AXI-Lite Control:  0x{:08X}
#   HLS AXI-Lite Address:  0x{:08X}
#   BRAM Memory:           0x{:08X}
# =====================================================================

puts "========================================"
puts "FPGA-Litho Board Validation Started"
puts "========================================"

# === Safe Hardware Connection ===
puts "INFO: Connecting to JTAG hardware..."

# Initialize hardware manager first (required before any hw_server operations)
open_hw_manager

# Smart connection management: check if already connected
set server_exists [catch {{get_hw_servers}}]
if {{$server_exists == 0}} {{
    puts "INFO: Hardware server already connected, disconnecting..."
    disconnect_hw_server [get_hw_servers]
}}

# Clean up any existing target connections
catch {{close_hw_target -quiet}}

# Connect to hardware server and target
puts "INFO: Connecting to hardware server localhost:3121..."
connect_hw_server -url localhost:3121
open_hw_target

# Program the device
current_hw_device [lindex [get_hw_devices] 0]
program_hw_devices [current_hw_device]
refresh_hw_device [current_hw_device]
puts "INFO: FPGA programmed successfully"

# Reset AXI Master
puts "INFO: Resetting JTAG-to-AXI Master..."
reset_hw_axi [get_hw_axis hw_axi_1]
puts "INFO: AXI Master reset complete"

'''.format(hls_ctrl, hls_ctrl_r, bram_base)
    
    # Step 3: Write Input Data to BRAM
    tcl_content += '''# =====================================================================
# Step 3: Write Input Data to BRAM via JTAG-to-AXI
# =====================================================================
puts "INFO: Writing input data to BRAM..."

'''
    
    # Write mskf_r.bin
    mskf_r_file = GOLDEN_DIR / 'mskf_r.bin'
    if mskf_r_file.exists():
        cmds, words, txns = generate_write_transactions_for_file(mskf_r_file, ddr_addrs['mskf_r'])
        tcl_content += f'# Writing mskf_r.bin ({words} floats, {txns} transactions)\n'
        tcl_content += ''.join(cmds)
        tcl_content += '\n'
    else:
        print(f"[WARNING] File not found: {mskf_r_file}")
    
    # Write mskf_i.bin
    mskf_i_file = GOLDEN_DIR / 'mskf_i.bin'
    if mskf_i_file.exists():
        cmds, words, txns = generate_write_transactions_for_file(mskf_i_file, ddr_addrs['mskf_i'])
        tcl_content += f'# Writing mskf_i.bin ({words} floats, {txns} transactions)\n'
        tcl_content += ''.join(cmds)
        tcl_content += '\n'
    
    # Write scales.bin
    scales_file = GOLDEN_DIR / 'scales.bin'
    if scales_file.exists():
        cmds, words, txns = generate_write_transactions_for_file(scales_file, ddr_addrs['scales'])
        tcl_content += f'# Writing scales.bin ({words} floats, {txns} transactions)\n'
        tcl_content += ''.join(cmds)
        tcl_content += '\n'
    
    # Write kernels (10 files)
    kernels_dir = GOLDEN_DIR / 'kernels'
    if kernels_dir.exists():
        # Kernel实部
        for i in range(10):
            krn_r_file = kernels_dir / f'krn_{i}_r.bin'
            if krn_r_file.exists():
                # Each kernel is 9×9 = 81 floats, offset per kernel
                krn_addr = ddr_addrs['krn_r'] + i * 81 * 4
                cmds, words, txns = generate_write_transactions_for_file(krn_r_file, krn_addr)
                tcl_content += f'# Writing krn_{i}_r.bin ({words} floats, {txns} transactions)\n'
                tcl_content += ''.join(cmds)
        
        # Kernel虚部
        for i in range(10):
            krn_i_file = kernels_dir / f'krn_{i}_i.bin'
            if krn_i_file.exists():
                krn_addr = ddr_addrs['krn_i'] + i * 81 * 4
                cmds, words, txns = generate_write_transactions_for_file(krn_i_file, krn_addr)
                tcl_content += f'# Writing krn_{i}_i.bin ({words} floats, {txns} transactions)\n'
                tcl_content += ''.join(cmds)
    
    # Step 4: Configure HLS Address Registers
    tcl_content += f'''# =====================================================================
# Step 4: Configure HLS Address Registers
# =====================================================================
puts "INFO: Configuring HLS address registers..."

set HLS_CTRL_R_BASE 0x{hls_ctrl_r:08X}

'''
    
    # Write DDR pointers to HLS registers (offsets: 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14)
    register_mapping = [
        ('mskf_r', 0x00),
        ('mskf_i', 0x04),
        ('scales', 0x08),
        ('krn_r',  0x0C),
        ('krn_i',  0x10),
        ('output', 0x14),
    ]
    
    for name, offset in register_mapping:
        addr = ddr_addrs[name]
        reg_addr = hls_ctrl_r + offset
        tcl_content += f'''# Configure {name} pointer register
create_hw_axi_txn write_{name}_ptr [get_hw_axis hw_axi_1] \
    -type WRITE \
    -address 0x{reg_addr:08X} \
    -data {{0x{addr:08X}}} -len 1
run_hw_axi [get_hw_axi_txns write_{name}_ptr]

'''
    
    tcl_content += 'puts "INFO: Address registers configured"\n\n'
    
    # Step 5: Start HLS IP
    tcl_content += f'''# =====================================================================
# Step 5: Start HLS IP Execution
# =====================================================================
puts "INFO: Starting HLS IP (ap_start)..."

set HLS_CTRL_BASE 0x{hls_ctrl:08X}

# Write ap_start = 1 (bit[0] of AP_CTRL register)
create_hw_axi_txn start_hls [get_hw_axis hw_axi_1] \
    -type WRITE \
    -address 0x{hls_ctrl:08X} \
    -data {{0x00000001}} -len 1
run_hw_axi [get_hw_axi_txns start_hls]

puts "INFO: HLS IP started"

'''
    
    # Step 6: Poll ap_done
    tcl_content += '''# =====================================================================
# Step 6: Poll ap_done with Timeout
# =====================================================================
puts "INFO: Waiting for HLS completion..."

set timeout_ms 5000
set start_time [clock milliseconds]
set done 0

while {!$done} {
    # Read AP_CTRL register
    create_hw_axi_txn read_status [get_hw_axis hw_axi_1] \
        -type READ \
        -address $HLS_CTRL_BASE -len 1
    run_hw_axi [get_hw_axi_txns read_status]
    
    # Extract status value (bit[1] = ap_done)
    set status_raw [get_property DATA [get_hw_axi_txn read_status]]
    
    # Parse first word from status string
    set status_word [lindex $status_raw 0]
    set status_val [expr {$status_word}]
    
    if {[expr {$status_val & 0x02}] != 0} {
        set done 1
        puts "INFO: HLS execution completed (ap_done=1)"
    }
    
    # Timeout check
    set elapsed [expr {[clock milliseconds] - $start_time}]
    if {$elapsed > $timeout_ms} {
        puts "ERROR: Timeout after $timeout_ms ms"
        puts "ERROR: Last status value: $status_val"
        exit 1
    }
    
    # Wait 10ms before next poll
    after 10
}

'''
    
    # Step 7: Read Output Data
    output_addr = ddr_addrs['output']
    output_len = 289  # 17×17 floats
    
    tcl_content += f'''# =====================================================================
# Step 7: Read Output Data from BRAM
# =====================================================================
puts "INFO: Reading output data..."

set OUTPUT_ADDR 0x{output_addr:08X}
set OUTPUT_LEN {output_len}

create_hw_axi_txn read_output [get_hw_axis hw_axi_1] \
    -type READ \
    -address $OUTPUT_ADDR \
    -len $OUTPUT_LEN
run_hw_axi [get_hw_axi_txns read_output]

# Extract output data
set output_raw [get_property DATA [get_hw_axi_txn read_output]]
puts "INFO: Output data captured ($OUTPUT_LEN floats)"

# Save output to file for PC analysis
set output_file "board_output.txt"
set fp [open $output_file w]
puts $fp $output_raw
close $fp
puts "INFO: Output saved to $output_file"

'''
    
    # Completion message
    tcl_content += '''# =====================================================================
# Validation Complete
# =====================================================================
puts "========================================"
puts "Board Validation Completed Successfully"
puts "========================================"
puts "Next steps on PC:"
puts "  1. Copy board_output.txt to PC"
puts "  2. Run compare_results.py to validate against Golden"
puts "Golden file: output/verification/tmpImgp_pad32.bin"
puts "========================================"
'''
    
    # Write TCL file
    with open(output_file, 'w') as f:
        f.write(tcl_content)
    
    print(f"\n[SUCCESS] Generated TCL script: {output_file}")
    print(f"  HLS Control:  0x{hls_ctrl:08X}")
    print(f"  HLS Address:  0x{hls_ctrl_r:08X}")
    print(f"  BRAM Base:    0x{bram_base:08X}")
    print(f"  Output Addr:  0x{output_addr:08X}")

# =====================================================================
# Main Execution
# =====================================================================

def main():
    print("=" * 60)
    print("JTAG-to-AXI TCL Script Generator")
    print("=" * 60)
    
    # Check files exist
    if not CSV_FILE.exists():
        print(f"[ERROR] AddressSegments.csv not found: {CSV_FILE}")
        return
    
    if not GOLDEN_DIR.exists():
        print(f"[ERROR] Golden data directory not found: {GOLDEN_DIR}")
        return
    
    # Parse address mapping
    print("\n[STEP 1] Parsing AddressSegments.csv...")
    addresses = parse_address_segments_csv(CSV_FILE)
    
    # Generate TCL script
    print("\n[STEP 2] Generating TCL script...")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    output_tcl = OUTPUT_DIR / 'run_board_validation_auto.tcl'
    generate_full_tcl_script(addresses, output_tcl)
    
    print("\n" + "=" * 60)
    print("Generation Complete!")
    print("=" * 60)
    print("\nUsage:")
    print("  1. Vivado Hardware Manager: source run_board_validation_auto.tcl")
    print("  2. Or batch mode: vivado -mode batch -source run_board_validation_auto.tcl")
    print("  3. PC validation: python compare_results.py")

if __name__ == '__main__':
    main()