#!/usr/bin/env python3
"""Quick FI validation - match FFTW R2C/C2R half-spectrum."""

import numpy as np
from pathlib import Path

PROJECT_ROOT = Path("/home/ashington/fpga-litho-accel")

def my_shift(arr, shift_type_x=False, shift_type_y=False):
    """Match litho.cpp myShift behavior."""
    rows, cols = arr.shape
    xh = cols // 2 if shift_type_x else (cols + 1) // 2
    yh = rows // 2 if shift_type_y else (rows + 1) // 2
    out = np.zeros_like(arr)
    for y in range(rows):
        for x in range(cols):
            sx = (x + xh) % cols
            sy = (y + yh) % rows
            out[sy, sx] = arr[y, x]
    return out

def fourier_interpolation_fftw(input_img, output_size):
    """
    Match litho.cpp FI: FFTW R2C/C2R half-spectrum embedding.
    
    Key: FFTW R2C produces half-spectrum (sizeY × (sizeX/2+1)),
    numpy fft2 produces full spectrum (sizeY × sizeX).
    
    We need to simulate FFTW half-spectrum layout.
    """
    in_sx, in_sy = input_img.shape
    out_sx, out_sy = output_size
    difY = out_sy - in_sy
    
    # Step 1: myShift(false, false) - equivalent to moving DC to corner
    shifted_in = my_shift(input_img, False, False)
    
    # Step 2: FFT (simulate FFTW R2C half-spectrum)
    # FFTW R2C: input [sizeY × sizeX] → output [sizeY × (sizeX/2+1)]
    full_spectrum = np.fft.fft2(shifted_in) / (in_sx * in_sy)
    
    # Extract half-spectrum (FFTW R2C format)
    half_spectrum_in = full_spectrum[:, :in_sx//2 + 1]  # [in_sy × (in_sx/2+1)]
    
    # Step 3: Embed into larger half-spectrum
    # Output half-spectrum size: out_sy × (out_sx/2+1)
    half_spectrum_out = np.zeros((out_sy, out_sx//2 + 1), dtype=np.complex128)
    
    # Copy positive frequencies (y: 0..in_sy/2, x: 0..in_sx/2+1)
    half_spectrum_out[:in_sy//2 + 1, :in_sx//2 + 1] = half_spectrum_in[:in_sy//2 + 1, :]
    
    # Copy negative frequencies (y: in_sy/2+1..in_sy, x: 0..in_sx/2+1)
    # In FFTW half-spectrum, negative y maps to (y + difY)
    half_spectrum_out[(in_sy//2 + 1) + difY:, :in_sx//2 + 1] = half_spectrum_in[in_sy//2 + 1:, :]
    
    # Step 4: Convert half-spectrum back to full spectrum for numpy IFFT
    # FFTW C2R expects half-spectrum [out_sy × (out_sx/2+1)]
    # We need to reconstruct full spectrum [out_sy × out_sx]
    
    full_spectrum_out = np.zeros((out_sy, out_sx), dtype=np.complex128)
    
    # Copy half-spectrum to left half
    full_spectrum_out[:, :out_sx//2 + 1] = half_spectrum_out
    
    # Fill right half using Hermitian symmetry
    # For y in 0..out_sy-1, x in out_sx//2+1..out_sx-1:
    #   F[y, x] = conj(F[(out_sy - y) % out_sy, out_sx - x])
    for y in range(out_sy):
        for x in range(out_sx//2 + 1, out_sx):
            src_y = (out_sy - y) % out_sy
            src_x = out_sx - x
            full_spectrum_out[y, x] = np.conj(full_spectrum_out[src_y, src_x])
    
    # Step 5: IFFT
    result = np.fft.ifft2(full_spectrum_out) * (out_sx * out_sy)
    
    # Step 6: myShift(false, false) on result
    result_shifted = my_shift(np.abs(result), False, False)
    
    return result_shifted

# Load HLS 32x32 output
hls_32 = np.fromfile(PROJECT_ROOT / "source/SOCS_HLS/hls/data/hls_output_full_32.bin", dtype=np.float32).reshape(32, 32)
print(f"HLS 32x32 range: [{hls_32.min():.6f}, {hls_32.max():.6f}]")
print(f"HLS 32x32 mean: {hls_32.mean():.6f}")

# Apply Fourier Interpolation to 512x512
hls_512 = fourier_interpolation_fftw(hls_32, (512, 512))
print(f"HLS FI 512x512 range: [{hls_512.min():.6f}, {hls_512.max():.6f}]")
print(f"HLS FI 512x512 mean: {hls_512.mean():.6f}")

# Load golden aerial image
golden_512 = np.fromfile(PROJECT_ROOT / "output/verification/aerial_image_socs_kernel.bin", dtype=np.float32).reshape(512, 512)
print(f"Golden 512x512 range: [{golden_512.min():.6f}, {golden_512.max():.6f}]")
print(f"Golden 512x512 mean: {golden_512.mean():.6f}")

# Compute RMSE
rmse = np.sqrt(np.mean((hls_512 - golden_512)**2))
max_err = np.max(np.abs(hls_512 - golden_512))
print(f"FI RMSE: {rmse:.6e}")
print(f"FI Max Error: {max_err:.6e}")

# Check if RMSE < 1e-5
if rmse < 1e-5:
    print("✓ FI validation PASSED")
else:
    print(f"✗ FI validation FAILED (RMSE {rmse:.6e} > 1e-5)")
    
# Save FI output
hls_512.astype(np.float32).tofile(PROJECT_ROOT / "output/verification/hls_fi_512x512.bin")
print("Saved: output/verification/hls_fi_512x512.bin")
