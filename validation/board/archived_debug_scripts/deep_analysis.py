#!/usr/bin/env python3
"""
Deep analysis of HLS output vs Golden intensity
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
print("INTENSITY CALCULATION DEEP ANALYSIS")
print("=" * 70)

# Analysis 1: Is HLS output intensity or IFFT magnitude?
print("\n1. OUTPUT TYPE HYPOTHESIS:")
ratio = golden_tmp.mean() / hls_out.mean()
print(f"   Ratio (Golden/HLS): {ratio:.0f}")
print(f"   sqrt(ratio): {np.sqrt(ratio):.1f}")

# Hypothesis A: HLS outputs IFFT magnitude (not intensity)
# intensity = scale × magnitude^2 → magnitude = sqrt(intensity / scale)
avg_scale = scales.mean()
scale_max = scales.max()
scale_sum = scales.sum()

print(f"\n   Hypothesis A: HLS outputs |IFFT_output|:")
print(f"     Expected |IFFT| = sqrt(Golden/scale)")
print(f"     Using avg scale: sqrt({golden_tmp.mean():.6f}/{avg_scale:.4f}) = {np.sqrt(golden_tmp.mean()/avg_scale):.6f}")
print(f"     Using max scale: sqrt({golden_tmp.mean():.6f}/{scale_max:.4f}) = {np.sqrt(golden_tmp.mean()/scale_max):.6f}")
print(f"     HLS output: {hls_out.mean():.6e}")
print(f"     Match? No - HLS output is {np.sqrt(golden_tmp.mean()/scale_max)/hls_out.mean():.0f}x smaller")

# Hypothesis B: HLS outputs intensity but missing compensation
print(f"\n   Hypothesis B: HLS outputs intensity with missing scaling:")
print(f"     If HLS intensity × N_comp = Golden intensity")
print(f"     N_comp = Golden/HLS = {ratio:.0f}")
print(f"     Possible N_comp values:")
print(f"       32² = {32**2} (FFT scaling)")
print(f"       1024² = {1024**2} (FFT scaling)")
print(f"       scales.sum() = {scale_sum:.2f} (10 kernel accum)")
print(f"     Ratio / 32² = {ratio / 32**2:.1f}")
print(f"     Ratio / 1024 = {ratio / 1024:.1f}")

# Analysis 2: Check if HLS output matches any known FFT scaling pattern
print("\n2. FFT SCALING ANALYSIS:")
print("   Vitis HLS FFT scaling_options = scaled")
print("   For SCALED mode: output = IFFT_output / N")
print("   For UNSCALED mode: output = raw IFFT (no normalization)")

# Expected values for different modes
print("\n   Expected output magnitude for different FFT modes:")
# Input product magnitude (from previous analysis)
typical_product = 1.63e-3
ifft_raw = 81 * typical_product  # 9×9 contributions
print(f"     UNSCALED (raw IFFT): {ifft_raw:.6e}")
print(f"     SCALED (div by 32): {ifft_raw / 32:.6e}")
print(f"     HLS actual output: {hls_out.mean():.6e}")
print(f"     Ratio (UNSCALED/HLS): {ifft_raw / hls_out.mean():.0f}")
print(f"     Ratio (SCALED/HLS): {(ifft_raw / 32) / hls_out.mean():.0f}")

# Analysis 3: Intensity calculation formula check
print("\n3. INTENSITY FORMULA CHECK:")
print("   HLS code: intensity = scale × (re² + im²)")
print("   This matches litho.cpp Golden formula")

# But if HLS output is already intensity, check if formula is wrong
# intensity = scale × |IFFT|^2
# If HLS uses |IFFT|^2 but Golden uses sqrt(|IFFT|), we'd see ratio

# Try reverse: what would Golden be if formula was different?
print("\n   Reverse calculation:")
print(f"     If HLS output {hls_out.mean():.6e} is |IFFT|^2:")
print(f"       Then scale × HLS = {scale_max:.4f} × {hls_out.mean():.6e} = {scale_max * hls_out.mean():.6e}")
print(f"       Golden intensity: {golden_tmp.mean():.6e}")
print(f"       Ratio: {golden_tmp.mean() / (scale_max * hls_out.mean()):.0f}")

print(f"\n     If HLS output {hls_out.mean():.6e} is sqrt(intensity):")
print(f"       Then HLS² = {hls_out.mean()**2:.6e}")
print(f"       Golden intensity: {golden_tmp.mean():.6e}")
print(f"       Ratio: {golden_tmp.mean() / hls_out.mean()**2:.0f}")

# Analysis 4: Check if scales are loaded correctly
print("\n4. SCALES LOADING CHECK:")
expected_scales_hex = [format(struct.unpack('>I', struct.pack('>f', s))[0], '08X') for s in scales[:4]]
print(f"   Expected scales hex (first 4): {expected_scales_hex}")
print(f"   Scales values: {scales[:4]}")

# Analysis 5: Find the magic number
print("\n5. FINDING THE MAGIC COMPENSATION:")
# Ratio = 30653
# We need to find what combination produces this

test_factors = [
    ("32 (FFT length)", 32),
    ("1024 (32²)", 1024),
    ("1048576 (1024²)", 1024**2),
    ("scale_sum", scale_sum),
    ("scale_max", scale_max),
    ("81 (9×9)", 81),
    ("289 (17×17)", 289),
]

print(f"   Target ratio: {ratio:.0f}")
for name, factor in test_factors:
    print(f"     ratio / {name} = {ratio / factor:.1f}")

# Check product combinations
print("\n6. COMBINATION ANALYSIS:")
print(f"   32² × scale_sum = {32**2 * scale_sum:.0f} (ratio={ratio:.0f})")
print(f"   32 × scale_max = {32 * scale_max:.1f}")
print(f"   1024 × scale_sum = {1024 * scale_sum:.0f}")
print(f"   sqrt(ratio) × scale_sum = {np.sqrt(ratio) * scale_sum:.1f}")
print(f"   sqrt(ratio) × 32 = {np.sqrt(ratio) * 32:.0f}")

# Most likely: ratio ≈ 30 × 1024
if abs(ratio - 30 * 1024) < abs(ratio - 30*1024) * 0.1:
    print("\n   *** BEST MATCH: ratio ≈ 30 × 1024 = 30672 ***")
    print("   Interpretation:")
    print("     1024 = FFT scaling (32² for SCALED mode)")
    print("     30 = ??? (maybe kernel count × avg_scale = 10 × 3?)")
    print("   Hypothesis: HLS FFT uses SCALED mode AND intensity formula is wrong")

print("\n" + "=" * 70)
print("DIAGNOSIS SUMMARY")
print("=" * 70)
print(f"Ratio {ratio:.0f} = 29.93 × 1024")
print("Most likely explanation:")
print("  1. FFT uses SCALED mode (div by N=32 per dimension → 1024 total)")
print("  2. Additional 30× mismatch from intensity formula or kernel accumulation")
print("\nRecommendation:")
print("  1. Verify HLS FFT actually uses UNSCALED mode (check IP config)")
print("  2. Add N²=1024 output compensation if FFT is SCALED")
print("  3. Check intensity formula: is it using |IFFT|² or something else?")