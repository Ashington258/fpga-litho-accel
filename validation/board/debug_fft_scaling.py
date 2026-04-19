#!/usr/bin/env python3
"""
Debug FFT scaling and fixed-point precision issue

Hypothesis: HLS fixed-point FFT loses precision for small input values
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

# Load input data
print("=" * 70)
print("FIXED-POINT FFT PRECISION ANALYSIS")
print("=" * 70)

mskf_r = read_bin('output/verification_original/mskf_r.bin')
mskf_i = read_bin('output/verification_original/mskf_i.bin')
scales = read_bin('output/verification_original/scales.bin')

# Load kernels from individual files
krn_r_list = []
krn_i_list = []
for k in range(10):
    krn_r_list.append(read_bin(f'output/verification_original/kernels/krn_{k}_r.bin'))
    krn_i_list.append(read_bin(f'output/verification_original/kernels/krn_{k}_i.bin'))
krn_r = np.concatenate(krn_r_list)
krn_i = np.concatenate(krn_i_list)

print("\n1. Input Data Magnitude Analysis:")
print(f"   mskf_r: mean={np.abs(mskf_r).mean():.6e}, max={np.abs(mskf_r).max():.6e}")
print(f"   mskf_i: mean={np.abs(mskf_i).mean():.6e}, max={np.abs(mskf_i).max():.6e}")
print(f"   krn_r:  mean={np.abs(krn_r).mean():.6e}, max={np.abs(krn_r).max():.6e}")
print(f"   krn_i:  mean={np.abs(krn_i).mean():.6e}, max={np.abs(krn_i).max():.6e}")

# Sample product calculation (kernel × mask)
# Extract 9×9 kernel region centered at mask spectrum center
Lx, Ly = 512, 512
Nx, Ny = 4, 4
kerX, kerY = 2*Nx+1, 2*Ny+1  # 9×9
nk = 10

Lxh, Lyh = Lx//2, Ly//2

# Calculate typical product magnitude
print("\n2. Typical Product Magnitude (kernel × mask):")
product_mags = []
for k in range(min(3, nk)):  # Check first 3 kernels
    for ky in range(kerY):
        for kx in range(kerX):
            phys_y = ky - Ny
            phys_x = kx - Nx
            mask_y = Lyh + phys_y
            mask_x = Lxh + phys_x
            
            mask_idx = mask_y * Lx + mask_x
            kr_idx = k * kerX * kerY + ky * kerX + kx
            
            ms_r = mskf_r[mask_idx]
            ms_i = mskf_i[mask_idx]
            kr_r = krn_r[kr_idx]
            kr_i = krn_i[kr_idx]
            
            prod_r = kr_r * ms_r - kr_i * ms_i
            prod_i = kr_r * ms_i + kr_i * ms_r
            
            mag = np.sqrt(prod_r**2 + prod_i**2)
            product_mags.append(mag)

product_mags = np.array(product_mags)
print(f"   Product magnitude: mean={product_mags.mean():.6e}, max={product_mags.max():.6e}")
print(f"   Min product: {product_mags.min():.6e}")

# Q1.31 Fixed-Point Precision Analysis
print("\n3. Q1.31 Fixed-Point Precision:")
print("   Format: 1 integer bit + 31 fractional bits")
print("   Range: [-1.0, 0.999...]")
print("   Resolution: 2^-31 ≈ 4.66e-10")

# Check if typical values are above resolution
typical_product = product_mags.mean()
resolution = 2**-31
print(f"   Typical product: {typical_product:.6e}")
print(f"   Resolution: {resolution:.6e}")
print(f"   Ratio: {typical_product / resolution:.0f}")
print(f"   Representable? {typical_product > resolution}")

# If product < resolution, it's quantized to zero!
if typical_product < resolution:
    print("   *** CRITICAL: Product magnitude < resolution → Quantized to zero!")
else:
    # Calculate quantization error
    quantized_val = round(typical_product * 2**31) / 2**31
    error = abs(typical_product - quantized_val)
    print(f"   Quantized value: {quantized_val:.6e}")
    print(f"   Quantization error: {error:.6e} ({error/typical_product*100:.2f}%)")

# Check with INPUT_SCALE_FACTOR = 1.0 (current config)
print("\n4. HLS FFT Input with FFT_INPUT_SCALE_FACTOR = 1.0:")
scaled_product = typical_product * 1.0  # No scaling
print(f"   Scaled input: {scaled_product:.6e}")
print(f"   Fixed-point representation: {int(scaled_product * 2**31)} / 2^31")

# Check with INPUT_SCALE_FACTOR = 2^20 (recommended for small values)
print("\n5. Recommended Scaling: FFT_INPUT_SCALE_FACTOR = 2^20:")
scale_2_20 = 2**20
scaled_product_2_20 = typical_product * scale_2_20
print(f"   Scaled input: {scaled_product_2_20:.6e}")
print(f"   Fixed-point representation: {int(scaled_product_2_20 * 2**31)} / 2^31")
print(f"   Utilized bits: ~{int(np.log2(abs(scaled_product_2_20 * 2**31)))} bits")

# FFT Scaling Analysis
print("\n6. FFT Scaling Mode Analysis:")
print("   Config: scaling_options = scaled")
print("   Expected: FFT output divided by N (32)")
print("   But HLS actually uses UNSCALED mode (discovered in testing)")
print("   UNSCALED output = raw IFFT result (no 1/N normalization)")

# Calculate expected output magnitude
print("\n7. Expected IFFT Output Magnitude (32×32 IFFT):")
# For 32×32 IFFT with UNSCALED mode:
# Output = Σ input[i] × e^(j2πki/N) (no normalization)
# For centered 9×9 input with values ~typical_product
# Expected magnitude ≈ 9×9 × typical_product (sum of contributions)
expected_ifft_out = 81 * typical_product  # 9×9 kernel size
print(f"   Expected raw IFFT magnitude: {expected_ifft_out:.6e}")

# But HLS output shows ~1.47e-6, which is MUCH smaller!
print("\n8. HLS Output vs Expected:")
hls_output = read_bin('output/board/hls_output.bin')
golden_tmpImgp = read_bin('output/verification_original/tmpImgp_pad32.bin')

print(f"   HLS output mean: {hls_output.mean():.6e}")
print(f"   Golden tmpImgp mean: {golden_tmpImgp.mean():.6e}")
print(f"   Ratio: {golden_tmpImgp.mean() / hls_output.mean():.0f}")

# Reverse calculation: what input magnitude would produce HLS output?
reverse_input = hls_output.mean() / 81
print(f"\n9. Reverse Calculation (what input produces HLS output?):")
print(f"   If HLS output = 1.47e-6, input magnitude ≈ {reverse_input:.6e}")
print(f"   Actual input magnitude: {typical_product:.6e}")
print(f"   Ratio: {typical_product / reverse_input:.0f}")

# Check if this matches ~1024 (FFT scaling)
if typical_product / reverse_input > 900 and typical_product / reverse_input < 1100:
    print("   *** This matches FFT scaling factor N=32 (N² ≈ 1024)!")
    print("   *** Hypothesis: HLS FFT is using SCALED mode (divides by N²)")

# Check intensity calculation
print("\n10. Intensity Calculation Check:")
# intensity = scale × |IFFT_output|^2
# For scale = 5.76 (largest eigenvalue)
scale_max = scales.max()
intensity_from_hls = scale_max * hls_output.mean()**2
intensity_from_golden = golden_tmpImgp.mean()

print(f"   Scale (largest): {scale_max:.4f}")
print(f"   If HLS output is IFFT output:")
print(f"     Intensity = {scale_max} × ({hls_output.mean():.6e})² = {intensity_from_hls:.6e}")
print(f"   Golden intensity: {intensity_from_golden:.6e}")
print(f"   Ratio: {intensity_from_golden / intensity_from_hls:.0f}")

print("\n" + "=" * 70)
print("DIAGNOSIS:")
print("=" * 70)
print(f"Golden/HLS ratio: {golden_tmpImgp.mean() / hls_output.mean():.0f}")
print(f"sqrt(ratio): {np.sqrt(golden_tmpImgp.mean() / hls_output.mean()):.1f}")
print(f"ratio / 32: {(golden_tmpImgp.mean() / hls_output.mean()) / 32:.1f}")
print(f"ratio / 1024: {(golden_tmpImgp.mean() / hls_output.mean()) / 1024:.1f}")
print("\nPossible causes:")
print("1. FFT output is IFFT output (not intensity) → ratio ~175 (sqrt)")
print("2. FFT uses SCALED mode (div by 32) → ratio ~1024")
print("3. Intensity formula missing sqrt → ratio ~175")
print("4. Multiple scaling errors combined")