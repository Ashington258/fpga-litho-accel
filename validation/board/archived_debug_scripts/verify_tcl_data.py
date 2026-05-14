#!/usr/bin/env python3
"""Verify TCL data loading vs Golden data"""

import struct
import numpy as np

def hex_to_float_be(hex_str):
    """Convert hex string to float (big-endian, Vivado format)"""
    int_val = int(hex_str, 16)
    return struct.unpack('>f', struct.pack('>I', int_val))[0]

def read_bin(path):
    """Read binary float file"""
    with open(path, 'rb') as f:
        data = f.read()
    vals = []
    for i in range(0, len(data), 4):
        vals.append(struct.unpack('<f', data[i:i+4])[0])
    return np.array(vals)

# Parse TCL data from load_mskf_r.tcl (first batch)
print("=" * 70)
print("TCL DATA VERIFICATION")
print("=" * 70)

# Extract hex values from TCL script (first 128 words)
tcl_hex_batch0 = [
    "B8C80000", "B8B6EF49", "B88770AC", "B806749E", 
    "3686386C", "38192D4E", "3878DDB9", "3890CB93",
]

tcl_values = [hex_to_float_be(h) for h in tcl_hex_batch0]
print("\n1. TCL hex → float conversion (first 8 values):")
for h, v in zip(tcl_hex_batch0, tcl_values):
    print(f"   {h} → {v:.6e}")

# Load corresponding Golden values
golden_mskf_r = read_bin('output/verification_original/mskf_r.bin')
print("\n2. Golden mskf_r.bin values at same indices:")
for i, v in enumerate(golden_mskf_r[:8]):
    print(f"   Index {i}: {v:.6e}")

# Compare first 8 values
print("\n3. Comparison (TCL vs Golden):")
for i in range(8):
    tcl_val = tcl_values[i]
    golden_val = golden_mskf_r[i]
    diff = abs(tcl_val - golden_val)
    match = "✓" if diff < 1e-10 else "✗"
    print(f"   Index {i}: TCL={tcl_val:.6e}, Golden={golden_val:.6e}, diff={diff:.6e} {match}")

# Check if TCL data uses different encoding
print("\n4. Endianness Check:")
sample_hex = "B8C80000"
be_val = hex_to_float_be(sample_hex)
le_val = struct.unpack('<f', struct.pack('<I', int(sample_hex, 16)))[0]
print(f"   {sample_hex} → BE: {be_val:.6e}, LE: {le_val:.6e}")
print(f"   Golden index 0: {golden_mskf_r[0]:.6e}")

# Check if TCL script was generated from correct Golden data
print("\n5. TCL Script Generation Source:")
print("   If TCL hex matches Golden → data loading correct")
print("   If mismatch → TCL script generation error")

# Verify if first batch matches
if len(tcl_values) >= 8:
    batch_match = all(abs(tcl_values[i] - golden_mskf_r[i]) < 1e-10 for i in range(8))
    print(f"   First 8 values match: {batch_match}")

# Check statistics
print("\n6. Statistics Comparison:")
tcl_sample = np.array(tcl_values)
print(f"   TCL sample (8 vals): mean={tcl_sample.mean():.6e}")
print(f"   Golden full (262144 vals): mean={golden_mskf_r.mean():.6e}")
print(f"   Golden first 128: mean={golden_mskf_r[:128].mean():.6e}")

# If mismatch, find correct hex encoding
print("\n7. Reverse: What hex would encode Golden value?")
golden0 = golden_mskf_r[0]
golden0_hex_be = format(struct.unpack('>I', struct.pack('>f', golden0))[0], '08X')
golden0_hex_le = format(struct.unpack('<I', struct.pack('<f', golden0))[0], '08X')
print(f"   Golden[0] = {golden0:.6e}")
print(f"   → Big-endian hex: {golden0_hex_be}")
print(f"   → Little-endian hex: {golden0_hex_le}")
print(f"   TCL script uses: {tcl_hex_batch0[0]}")