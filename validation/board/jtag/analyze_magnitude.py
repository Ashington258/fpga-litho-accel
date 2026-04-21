#!/usr/bin/env python3
"""Analyze magnitude difference between HLS output and Golden reference"""

import struct
import math

# Read HLS output (skip comment lines)
hls_floats = []
with open('aerial_image_output.hex', 'r') as f:
    for line in f:
        line = line.strip()
        if line.startswith('#') or not line:
            continue
        hex_val = int(line, 16)
        float_val = struct.unpack('>f', struct.pack('>I', hex_val))[0]
        hls_floats.append(float_val)

print(f'HLS output: {len(hls_floats)} values')
print(f'Range: [{min(hls_floats):.6e}, {max(hls_floats):.6e}]')
print(f'First 5: {hls_floats[:5]}')

# Read Golden reference
golden_floats = []
with open('../../source/SOCS_HLS/data/tmpImgp_pad32.bin', 'rb') as f:
    golden_data = f.read()
    golden_floats = [struct.unpack('<f', golden_data[i:i+4])[0] 
                     for i in range(0, len(golden_data), 4)]

print(f'\nGolden output: {len(golden_floats)} values')
print(f'Range: [{min(golden_floats):.6e}, {max(golden_floats):.6e}]')
print(f'First 5: {golden_floats[:5]}')

# Calculate ratio
if hls_floats[0] != 0:
    ratio = golden_floats[0] / hls_floats[0]
    print(f'\nRatio (golden/hls): {ratio:.2f}')
    print(f'Log10 ratio: {math.log10(ratio):.2f}')
    
    # Check if ratio is consistent across all values
    ratios = [g/h if h != 0 else float('inf') for g, h in zip(golden_floats, hls_floats)]
    valid_ratios = [r for r in ratios if r != float('inf') and r > 0]
    if valid_ratios:
        avg_ratio = sum(valid_ratios) / len(valid_ratios)
        print(f'Average ratio: {avg_ratio:.2f}')
        print(f'Ratio std dev: {math.sqrt(sum((r-avg_ratio)**2 for r in valid_ratios)/len(valid_ratios)):.2f}')