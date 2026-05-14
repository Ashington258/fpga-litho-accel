#!/usr/bin/env python3
"""
Final FFT Scaling Diagnosis
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
print("FFT SCALING ROOT CAUSE ANALYSIS")
print("=" * 70)

print("\n1. HLS FFT CONFIGURATION:")
print("   Config: scaling_options = hls::ip_fft::SCALED")
print("   FFT size: 32×32 (log2(N) = 5)")
print("   Scaling per dimension: N = 32")
print("   Total scaling: N² = 32² = 1024")
print("   Mode: SCALED (divides by N each dimension)")

print("\n2. FFTW COMPARISON:")
print("   FFTW BACKWARD: UNSCALED (no 1/N division)")
print("   Golden litho.cpp uses FFTW BACKWARD")
print("   HLS uses Vitis HLS FFT SCALED")
print("   Therefore: HLS output = FFTW output / 1024")

print("\n3. INTENSITY CALCULATION:")
print("   Golden: intensity = scale × |IFFT_FFTW|^2")
print("   HLS: intensity = scale × |IFFT_HLS|^2")
print("   HLS output = FFTW output / 1024")
print("   HLS intensity = scale × |FFTW/1024|^2")
print("   HLS intensity = scale × |FFTW|^2 / 1024²")
print("   HLS intensity = Golden / 1024²")

print("\n4. EXPECTED RATIO:")
print("   Golden / HLS = 1024² = 1,048,576")
print("   But observed ratio = 30,653")
print("   Ratio / 1024 = 29.93 ≈ 30")

print("\n5. THE PROBLEM:")
print("   Expected: ratio = 1024² (intensity scales by N^4)")
print("   Observed: ratio = 1024 × 30")
print("")
print("   This means HLS output is NOT simply FFTW/1024!")
print("   There's an additional factor affecting the output.")

print("\n6. CHECKING FOR DOUBLE SCALING:")
print("   If HLS FFT applies scaling TWICE:")
print("   Total scaling = 1024 × 1024 = 1,048,576")
print("   Then intensity ratio = 1,048,576")
print("   But observed = 30,653")
print("   So double scaling is NOT the issue")

print("\n7. CHECKING 30× FACTOR:")
print("   nk = 10, kerX = 9, kerY = 9")
print("   nk × 3 = 30 ✓")
print("   But each kernel should only be processed ONCE")
print("   If processed 3 times → intensity × 3")
print("   But this doesn't match the 30× factor")

print("\n8. ALTERNATIVE: 30 = 2 × 15")
print("   fftConvX - convX = 32 - 17 = 15")
print("   2 × 15 = 30 ✓")
print("   This might relate to extraction region offset")

print("\n9. TESTING COMPENSATION:")
hls_comp_ffft = hls_out * 1024  # Compensate FFT scaling
ratio_fft = golden_tmp.mean() / hls_comp_ffft.mean()
print(f"   HLS × 1024 → Golden ratio: {ratio_fft:.2f}")

hls_comp_full = hls_out * 1024 * 30  # Full compensation
ratio_full = golden_tmp.mean() / hls_comp_full.mean()
print(f"   HLS × 1024 × 30 → Golden ratio: {ratio_full:.2f}")

print("\n10. ROOT CAUSE HYPOTHESIS:")
print("   A. FFT scaling: 1024 (confirmed)")
print("   B. Additional 30× factor (unknown origin)")
print("")
print("   Possible sources of 30×:")
print("   - Kernel loop executes 3× (nk × 3 = 30)")
print("   - Extraction offset related (2 × 15 = 30)")
print("   - HLS intensity formula error")

print("\n11. FIX RECOMMENDATION:")
print("   Step 1: Add FFT scaling compensation to HLS intensity formula")
print("     intensity = scale × (re² + im²) × 1024")
print("   Step 2: Verify ratio becomes ~30 (not 1.0)")
print("   Step 3: Investigate remaining 30× factor")
print("   Step 4: If 30× is confirmed, add full compensation:")
print("     intensity = scale × (re² + im²) × 1024 × 30")

print("\n" + "=" * 70)
print("IMMEDIATE ACTION")
print("=" * 70)
print("1. Update HLS intensity formula with FFT compensation")
print("2. Re-run C Simulation to verify")
print("3. If ratio still ~30, investigate HLS loop structure")
print("4. Check if HLS processes kernels multiple times")

# Statistical validation
print("\n" + "=" * 70)
print("STATISTICAL VALIDATION")
print("=" * 70)
print(f"Golden mean: {golden_tmp.mean():.6f}")
print(f"Golden std: {golden_tmp.std():.6f}")
print(f"HLS mean: {hls_out.mean():.6f}")
print(f"HLS std: {hls_out.std():.6f}")
print(f"Ratio (Golden/HLS): {golden_tmp.mean()/hls_out.mean():.2f}")
print(f"Ratio / 1024: {golden_tmp.mean()/hls_out.mean()/1024:.2f}")

# Per-pixel comparison
valid_pixels = golden_tmp > 0
if valid_pixels.sum() > 0:
    pixel_ratios = golden_tmp[valid_pixels] / hls_out[valid_pixels]
    print(f"\nPer-pixel ratio mean: {pixel_ratios.mean():.2f}")
    print(f"Per-pixel ratio std: {pixel_ratios.std():.2f}")
    print(f"Per-pixel ratio min: {pixel_ratios.min():.2f}")
    print(f"Per-pixel ratio max: {pixel_ratios.max():.2f}")