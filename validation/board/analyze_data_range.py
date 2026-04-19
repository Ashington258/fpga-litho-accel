#!/usr/bin/env python3
"""Analyze data ranges for debugging 30,000x amplitude mismatch"""

import struct
import numpy as np

def hex_to_float_be(hex_str):
    """Convert hex string to float (big-endian)"""
    int_val = int(hex_str, 16)
    return struct.unpack('>f', struct.pack('>I', int_val))[0]

def read_bin_file(path):
    """Read binary float file"""
    with open(path, 'rb') as f:
        data = f.read()
    vals = []
    for i in range(0, len(data), 4):
        vals.append(struct.unpack('<f', data[i:i+4])[0])
    return np.array(vals)

# Check TCL input data (from load_mskf_r.tcl)
print("=" * 60)
print("1. TCL Input Data Analysis (mskf_r)")
print("=" * 60)

# Sample hex values from TCL load script
sample_hex = [
    'B8C80000', 'B8B6EF49', 'B88770AC', 'B806749E',  # Batch 0 starts
    '3686386C', '38192D4E', '3878DDB9', '3890CB93',  # Positive values
]

for h in sample_hex:
    val = hex_to_float_be(h)
    print(f"  {h} => {val:.6e}")

# Check Golden input data
print("\n" + "=" * 60)
print("2. Golden mskf_r.bin Analysis")
print("=" * 60)

golden_mskf = read_bin_file('output/verification_original/mskf_r.bin')
print(f"  Count: {len(golden_mskf)}")
print(f"  Mean:  {golden_mskf.mean():.6e}")
print(f"  Std:   {golden_mskf.std():.6e}")
print(f"  Min:   {golden_mskf.min():.6e}")
print(f"  Max:   {golden_mskf.max():.6e}")
print(f"  Range: [1e-9, 1e-2]? -> {golden_mskf.min() > 1e-9 and golden_mskf.max() < 0.1}")

# Check scales
print("\n" + "=" * 60)
print("3. Golden scales.bin Analysis")
print("=" * 60)

golden_scales = read_bin_file('output/verification_original/scales.bin')
print(f"  Count: {len(golden_scales)} (nk={len(golden_scales)} kernels)")
print(f"  Values: {golden_scales}")
print(f"  Sum:    {golden_scales.sum():.6f}")

# Check HLS output
print("\n" + "=" * 60)
print("4. HLS Output Analysis")
print("=" * 60)

hls_out = read_bin_file('output/board/hls_output.bin')
print(f"  Count: {len(hls_out)}")
print(f"  Mean:  {hls_out.mean():.6e}")
print(f"  Std:   {hls_out.std():.6e}")
print(f"  Min:   {hls_out.min():.6e}")
print(f"  Max:   {hls_out.max():.6e}")

# Check Golden tmpImgp
print("\n" + "=" * 60)
print("5. Golden tmpImgp_pad32.bin Analysis")
print("=" * 60)

golden_tmp = read_bin_file('output/verification_original/tmpImgp_pad32.bin')
print(f"  Count: {len(golden_tmp)}")
print(f"  Mean:  {golden_tmp.mean():.6e}")
print(f"  Std:   {golden_tmp.std():.6e}")
print(f"  Min:   {golden_tmp.min():.6e}")
print(f"  Max:   {golden_tmp.max():.6e}")

# Ratio analysis
print("\n" + "=" * 60)
print("6. Ratio Analysis")
print("=" * 60)
ratio = golden_tmp.mean() / hls_out.mean()
print(f"  Golden/HLS ratio: {ratio:.2f}")
print(f"  Expected if correct: ~1.0")
print(f"  FFT scaling mismatch: ~1024")
print(f"  Wrong comparison target: ~262144")
print(f"  INPUT_SCALE=2^20 issue: ~1,048,576")

# Check input scale factor in code
print("\n" + "=" * 60)
print("7. Hypothesis Check")
print("=" * 60)
print(f"  Ratio = {ratio:.0f}")
print(f"  sqrt({ratio:.0f}) = {np.sqrt(ratio):.2f}")
print(f"  {ratio:.0f} / 1024 = {ratio/1024:.2f}")
print(f"  {ratio:.0f} / 32 = {ratio/32:.2f}")