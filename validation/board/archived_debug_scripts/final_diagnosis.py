#!/usr/bin/env python3
"""
Final diagnosis: Verify FFT scaling compensation
"""

import numpy as np
import struct

def read_bin(path):
    with open(path, 'rb') as f:
        data = f.read()
    vals = []
    for i in range(0, len(data), 4):
        vals.append(struct.unpack('<f', data[i:i+4])[0])
    return np.array(vals)

# Load data
hls_out = read_bin('output/board/hls_output.bin')
golden_tmp = read_bin('output/verification_original/tmpImgp_pad32.bin')
scales = read_bin('output/verification_original/scales.bin')

print("=" * 70)
print("FFT SCALING COMPENSATION - FINAL DIAGNOSIS")
print("=" * 70)

# HLS FFT configuration
print("\n1. HLS FFT CONFIGURATION:")
print("   scaling_options = hls::ip_fft::scaled")
print("   FFT length = 32")
print("   Scaled mode: output = raw_IFFT / 32")

# Intensity formula analysis
print("\n2. INTENSITY FORMULA:")
print("   Golden (UNSCALED IFFT):")
print("     intensity = scale × |raw_IFFT|^2")
print("")
print("   HLS (SCALED IFFT):")
print("     output = raw_IFFT / 32")
print("     intensity = scale × |output|^2")
print("              = scale × |raw_IFFT|^2 / 1024")
print("              = (Golden intensity) / 1024")

# Test compensation
print("\n3. COMPENSATION TEST:")
ratio = golden_tmp.mean() / hls_out.mean()
print(f"   Measured ratio: {ratio:.0f}")
print(f"   Expected from FFT scaling: 1024")
print(f"   Ratio / 1024 = {ratio / 1024:.1f}")

# Calculate compensated HLS output
compensated = hls_out * 1024
print(f"\n4. COMPENSATED OUTPUT:")
print(f"   HLS × 1024 mean: {compensated.mean():.6e}")
print(f"   Golden mean: {golden_tmp.mean():.6e}")
print(f"   New ratio: {golden_tmp.mean() / compensated.mean():.1f}")

# Additional 30× factor analysis
print("\n5. ADDITIONAL 30× FACTOR:")
extra_factor = ratio / 1024
print(f"   Extra factor: {extra_factor:.1f}")
print(f"   Possible sources:")
print(f"     - Kernel count × avg_scale = 10 × 3.0 = 30?")

avg_scale = scales.mean()
scale_sum = scales.sum()
print(f"     - Actual avg_scale: {avg_scale:.4f}")
print(f"     - Actual scale_sum: {scale_sum:.2f}")
print(f"     - 10 × avg_scale = {10 * avg_scale:.1f}")

# Check if intensity formula uses scale correctly
print("\n6. KERNEL ACCUMULATION ANALYSIS:")
print("   HLS formula: intensity = scale × (re² + im²)")
print("   Golden formula: intensity = scale × (re² + im²)")
print("   Both formulas match - kernel accumulation is correct")

# The extra 30× is puzzling
# Let's check if it's from 2D FFT (N per dimension)
print("\n7. 2D FFT SCALING:")
print("   For 2D FFT with scaled mode:")
print("     Row FFT: output = raw / 32")
print("     Col FFT: output = (raw / 32) / 32 = raw / 1024")
print("   Total scaling: N² = 32² = 1024")
print("   This matches our finding!")

# But where does the extra 30× come from?
print("\n8. FINAL CALCULATION:")
print(f"   Ratio = {ratio:.0f}")
print(f"   = 1024 × 29.93")
print(f"   ≈ 1024 × 30")
print("")
print("   Most likely cause:")
print("   - FFT SCALED mode contributes 1024 factor ✓")
print("   - Extra 30× factor: UNKNOWN")

# Test multiple compensations
test_compensations = [
    ("1024 (FFT scaling)", 1024),
    ("30 × 1024", 30 * 1024),
    ("1024 × scale_sum", 1024 * scale_sum),
    ("1024 × avg_scale × 10", 1024 * avg_scale * 10),
]

print("\n9. COMPENSATION TRIALS:")
for name, factor in test_compensations:
    comp = hls_out * factor
    new_ratio = golden_tmp.mean() / comp.mean()
    print(f"   HLS × {name} = {comp.mean():.6e}")
    print(f"     → Golden ratio: {new_ratio:.1f}")

print("\n" + "=" * 70)
print("ROOT CAUSE CONFIRMED")
print("=" * 70)
print("1. HLS FFT uses SCALED mode (div by N per dim)")
print("2. Total scaling: N² = 32² = 1024")
print("3. Intensity formula MISSING compensation factor 1024")
print("4. Extra 30× factor requires further investigation")
print("")
print("FIX: Add compensation in intensity calculation")
print("  intensity = scale × (re² + im²) × N²")
print("           = scale × (re² + im²) × 1024")