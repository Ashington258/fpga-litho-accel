#!/usr/bin/env python3
"""
Trace the mysterious 30× factor
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

# Load input data
mskf_r = read_bin('output/verification_original/mskf_r.bin')
mskf_i = read_bin('output/verification_original/mskf_i.bin')
krn_r = read_bin('output/verification_original/kernels/krn_0_r.bin')
krn_i = read_bin('output/verification_original/kernels/krn_0_i.bin')

print("=" * 70)
print("TRACING THE 30× FACTOR")
print("=" * 70)

# Analysis 1: Check scale values
print("\n1. SCALE VALUES:")
print(f"   scales = {scales}")
print(f"   scales.sum() = {scales.sum():.2f}")
print(f"   scales.mean() = {scales.mean():.4f}")
print(f"   scales.max() = {scales.max():.4f}")
print(f"   scales.min() = {scales.min():.4f}")

# Analysis 2: Check kernel indices
print("\n2. KERNEL INDEX COUNT:")
print(f"   nk (number of kernels) = 10")
print(f"   kerX = 9 (2*Nx+1)")
print(f"   kerY = 9 (2*Ny+1)")
print(f"   kerX × kerY = 81")

# Analysis 3: Check FFT dimensions
print("\n3. FFT DIMENSIONS:")
print(f"   fftConvX = 32")
print(f"   fftConvY = 32")
print(f"   convX = 17 (4*Nx+1)")
print(f"   convY = 17 (4*Ny+1)")

# Analysis 4: Check if 30 is related to Nx/Ny
print("\n4. Nx/Ny ANALYSIS:")
print(f"   Nx = 4, Ny = 4")
print(f"   4 × Ny = 16")
print(f"   4 × Nx = 16")
print(f"   kerX × Ny / Nx = 9 × 4 / 4 = 9")
print(f"   (kerX + kerY) / 2 = 9")

# Analysis 5: Check if 30 is from some combination
print("\n5. COMBINATION SEARCH:")
combinations = [
    ("nk × Ny", 10 * 4),
    ("nk × (Ny+1)", 10 * 5),
    ("kerX × Ny", 9 * 4),
    ("kerY × Nx", 9 * 4),
    ("(kerX + kerY) × Ny / 2", (9 + 9) * 4 / 2),
    ("nk × 3", 10 * 3),
    ("scales.sum() × Ny", scales.sum() * 4),
    ("scales.sum() × 2", scales.sum() * 2),
    ("scales.mean() × nk", scales.mean() * 10),
    ("scales.mean() × nk × 2", scales.mean() * 10 * 2),
]

for name, val in combinations:
    match = "✓" if abs(val - 30) < 1 else ""
    print(f"   {name} = {val:.1f} {match}")

# Analysis 6: Check if 30 relates to fftConvX - convX
print("\n6. DIMENSION DIFFERENCE:")
print(f"   fftConvX - convX = 32 - 17 = 15")
print(f"   fftConvY - convY = 32 - 17 = 15")
print(f"   (fftConvX - convX) × 2 = 30 ✓")

# Analysis 7: Check input data embedding region
print("\n7. INPUT EMBEDDING REGION:")
print(f"   Kernel embedding: BOTTOM-RIGHT corner")
print(f"   Embedding indices: difY to difY+kerY, difX to difX+kerX")
print(f"   difY = fftConvY - kerY = 32 - 9 = 23")
print(f"   difX = fftConvX - kerX = 32 - 9 = 23")
print(f"   Embedding region: (23,23) to (31,31)")
print(f"   Region size: 9×9 = 81")

# Analysis 8: Check valid output region extraction
print("\n8. OUTPUT EXTRACTION:")
print(f"   HLS extracts 17×17 from 32×32 (after fftshift)")
print(f"   Extraction offset: (32-17)/2 = 7")
print(f"   Extracted region: (7,7) to (23,23)")
print(f"   Region size: 17×17 = 289")

# Analysis 9: Check if 30 relates to output dimensions
print("\n9. OUTPUT SIZE ANALYSIS:")
print(f"   convX × convY = 289")
print(f"   convX + convY = 34")
print(f"   (convX + convY) / 2 = 17")

# Analysis 10: The real source of 30
print("\n10. MATHEMATICAL DERIVATION:")
print("   Ratio = 30653 = 1024 × 29.93 ≈ 1024 × 30")
print("")
print("   Where 30 comes from:")
print("   - FFT scaling: N² = 32² = 1024 ✓")
print("   - Extra factor: 30 = 2 × (fftConvX - convX) = 2 × 15 ✓")
print("")
print("   BUT WAIT! Why would dimension difference affect intensity?")
print("   This makes NO SENSE - intensity is per-pixel, not global scaling!")

# Alternative: Check HLS code for any hardcoded constant
print("\n11. ALTERNATIVE HYPOTHESIS:")
print("   Maybe HLS intensity formula has error in scale application?")
print("   If HLS uses: intensity = (re² + im²) / scale")
print("   But Golden uses: intensity = scale × (re² + im²)")
print("   Then ratio = scale² = avg_scale² = 1.31² = 1.71 (NOT 30)")

# Check if HLS output is actually |IFFT| not |IFFT|^2
print("\n12. OUTPUT TYPE CHECK:")
print("   If HLS output is |IFFT| (magnitude, not intensity):")
print(f"     Golden intensity = scale × |IFFT|²")
print(f"     If HLS output = |IFFT| / sqrt(scale)")
print(f"     Then intensity = scale × (HLS² × scale) = scale² × HLS²")
print(f"     Ratio = 1/scale² × Golden = 1/1.31² × 0.0449 = 0.033")
print(f"     This also doesn't match.")

# Most likely: HLS uses different intensity formula
print("\n13. CHECK HLS INTENSITY FORMULA:")
print("   HLS code comment: intensity = scale × (re² + im²)")
print("   BUT: This is INTENSITY formula, not magnitude formula")
print("")
print("   Possible error: HLS might compute intensity AFTER extraction")
print("   If HLS computes intensity on 17×17 instead of 32×32:")
print("   Then intensity would be 32×32/17×17 = 1024/289 = 3.5× smaller")
print("   NOT matching the observed 30× factor")

print("\n" + "=" * 70)
print("CONCLUSION")
print("=" * 70)
print("The 30× factor appears to be:")
print("  2 × (fftConvX - convX) = 2 × 15 = 30")
print("")
print("But this doesn't make sense mathematically!")
print("Intensity should be per-pixel, independent of array size.")
print("")
print("Recommendation:")
print("1. Add FFT scaling compensation (× 1024) FIRST")
print("2. Test HLS × 1024 output vs Golden")
print("3. If ratio still ≈ 30, investigate HLS intensity formula")
print("4. Check if HLS output is actually INTENSITY or MAGNITUDE")