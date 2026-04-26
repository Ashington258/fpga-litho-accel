#!/usr/bin/env python3
"""
Complete Validation Pipeline: HLS → tmpImgp → Fourier Interpolation → Aerial Image

This script performs end-to-end validation:
1. Compare HLS output with Golden tmpImgp (128×128)
2. Visualize tmpImgp comparison
3. Perform Fourier Interpolation (128×128 → 1024×1024)
4. Compare FI result with Golden aerial image
5. Visualize FI comparison

Usage:
    python validation/complete_validation_pipeline.py --config input/config/golden_1024.json
"""

import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib
import json
import argparse
import sys

matplotlib.use("Agg")

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def load_config(config_path: Path) -> dict:
    """Load JSON configuration"""
    with open(config_path) as f:
        return json.load(f)


def load_binary_data(filepath: Path, dtype=np.float32) -> np.ndarray:
    """Load binary data file"""
    if not filepath.exists():
        raise FileNotFoundError(f"File not found: {filepath}")
    return np.fromfile(str(filepath), dtype=dtype)


def my_shift(arr, shift_type_x=False, shift_type_y=False):
    """
    Match litho.cpp myShift behavior exactly.
    For even sizes: False → offset = size//2 + 1, True → offset = size//2
    """
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


def fourier_interpolation_128_to_1024(tmpImgp: np.ndarray) -> np.ndarray:
    """
    Perform Fourier Interpolation from 128×128 to 1024×1024.
    
    Steps (matching verified verify_fourier_interpolation.py):
    1. ifftshift - move DC from center to corner
    2. FFT (full spectrum) - get complex spectrum
    3. Normalize spectrum
    4. Zero-pad spectrum to 1024×1024
    5. IFFT - get 1024×1024 aerial image
    6. fftshift - move DC to center
    7. Scale by 1024×1024 (FFTW normalization)
    """
    import numpy.fft as fft
    
    in_size = 128
    out_size = 1024
    
    print(f"\n[Fourier Interpolation] {in_size}×{in_size} → {out_size}×{out_size}")
    
    # Step 1: ifftshift - move DC from center to corner
    tmpImgp_shifted = fft.ifftshift(tmpImgp)
    print(f"  Step 1: ifftshift (DC to corner)")
    
    # Step 2: FFT (full spectrum)
    spectrum = fft.fft2(tmpImgp_shifted)
    print(f"  Step 2: FFT → spectrum shape {spectrum.shape}")
    
    # Step 3: Normalize spectrum
    spectrum /= (in_size * in_size)
    print(f"  Step 3: Normalize spectrum")
    
    # Step 4: Zero-pad spectrum (full spectrum layout)
    half_x = in_size // 2  # 64
    half_y = in_size // 2  # 64
    dif = out_size - in_size  # 896
    
    spectrum_padded = np.zeros((out_size, out_size), dtype=np.complex128)
    
    # Positive-positive quadrant: [0..half_y, 0..half_x]
    spectrum_padded[:half_y+1, :half_x+1] = spectrum[:half_y+1, :half_x+1]
    
    # Positive-negative quadrant: [0..half_y, half_x+1..end]
    spectrum_padded[:half_y+1, half_x+1+dif:] = spectrum[:half_y+1, half_x+1:]
    
    # Negative-positive quadrant: [half_y+1..end, 0..half_x]
    spectrum_padded[half_y+1+dif:, :half_x+1] = spectrum[half_y+1:, :half_x+1]
    
    # Negative-negative quadrant: [half_y+1..end, half_x+1..end]
    spectrum_padded[half_y+1+dif:, half_x+1+dif:] = spectrum[half_y+1:, half_x+1:]
    
    print(f"  Step 4: Zero-pad spectrum → {spectrum_padded.shape}")
    
    # Step 5: IFFT
    aerial = fft.ifft2(spectrum_padded).real
    print(f"  Step 5: IFFT → aerial shape {aerial.shape}")
    
    # Step 6: fftshift - move DC from corner to center
    aerial_shifted = fft.fftshift(aerial)
    print(f"  Step 6: fftshift (DC to center)")
    
    # Step 7: Scale by out_size × out_size (FFTW normalization)
    aerial_scaled = aerial_shifted * (out_size * out_size)
    print(f"  Step 7: Scale by {out_size}×{out_size}")
    
    return aerial_scaled


def compare_and_visualize_tmpImgp(hls_output: np.ndarray, golden_tmpImgp: np.ndarray, output_dir: Path, output_size: int = 33):
    """Compare HLS output with Golden tmpImgp and visualize"""
    
    print("\n" + "=" * 70)
    print(f"Step 1: HLS Output vs Golden tmpImgp Comparison ({output_size}×{output_size})")
    print("=" * 70)
    
    # Statistics
    print("\n[Statistics]")
    print(f"  HLS Output:")
    print(f"    Range: [{hls_output.min():.6f}, {hls_output.max():.6f}]")
    print(f"    Mean:  {hls_output.mean():.6f}")
    print(f"    Std:   {hls_output.std():.6f}")
    
    print(f"  Golden tmpImgp:")
    print(f"    Range: [{golden_tmpImgp.min():.6f}, {golden_tmpImgp.max():.6f}]")
    print(f"    Mean:  {golden_tmpImgp.mean():.6f}")
    print(f"    Std:   {golden_tmpImgp.std():.6f}")
    
    # RMSE
    diff = hls_output - golden_tmpImgp
    rmse = np.sqrt(np.mean(diff ** 2))
    max_diff = np.max(np.abs(diff))
    relative_error = rmse / np.mean(np.abs(golden_tmpImgp)) * 100
    
    print(f"\n[Comparison]")
    print(f"  RMSE:            {rmse:.6e}")
    print(f"  Max Difference:  {max_diff:.6e}")
    print(f"  Relative Error:  {relative_error:.4f}%")
    
    if rmse < 1e-5:
        print(f"  Status:          ✅ PASS (RMSE < 1e-5)")
    else:
        print(f"  Status:          ❌ FAIL (RMSE >= 1e-5)")
    
    # Visualization
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    
    # Row 1: Heatmaps
    im1 = axes[0, 0].imshow(hls_output, cmap='hot', aspect='equal')
    axes[0, 0].set_title('HLS Output (tmpImgp)', fontsize=12, fontweight='bold')
    axes[0, 0].set_xlabel('X (pixels)')
    axes[0, 0].set_ylabel('Y (pixels)')
    plt.colorbar(im1, ax=axes[0, 0], fraction=0.046, pad=0.04)
    
    im2 = axes[0, 1].imshow(golden_tmpImgp, cmap='hot', aspect='equal')
    axes[0, 1].set_title('Golden tmpImgp', fontsize=12, fontweight='bold')
    axes[0, 1].set_xlabel('X (pixels)')
    axes[0, 1].set_ylabel('Y (pixels)')
    plt.colorbar(im2, ax=axes[0, 1], fraction=0.046, pad=0.04)
    
    im3 = axes[0, 2].imshow(np.abs(diff), cmap='hot', aspect='equal')
    axes[0, 2].set_title(f'Absolute Difference\nMax: {max_diff:.2e}', fontsize=12, fontweight='bold')
    axes[0, 2].set_xlabel('X (pixels)')
    axes[0, 2].set_ylabel('Y (pixels)')
    plt.colorbar(im3, ax=axes[0, 2], fraction=0.046, pad=0.04)
    
    # Row 2: Profiles and histogram
    center_y = hls_output.shape[0] // 2
    axes[1, 0].plot(hls_output[center_y, :], 'b-', linewidth=2, label='HLS')
    axes[1, 0].plot(golden_tmpImgp[center_y, :], 'r--', linewidth=2, label='Golden')
    axes[1, 0].set_title('Center Row Profile (Y=64)', fontsize=12, fontweight='bold')
    axes[1, 0].set_xlabel('X (pixels)')
    axes[1, 0].set_ylabel('Intensity')
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    
    axes[1, 1].hist(hls_output.flatten(), bins=50, alpha=0.7, label='HLS', color='blue')
    axes[1, 1].hist(golden_tmpImgp.flatten(), bins=50, alpha=0.7, label='Golden', color='red')
    axes[1, 1].set_title('Intensity Distribution', fontsize=12, fontweight='bold')
    axes[1, 1].set_xlabel('Intensity')
    axes[1, 1].set_ylabel('Frequency')
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)
    
    # Scatter plot
    axes[1, 2].scatter(golden_tmpImgp.flatten()[::100], hls_output.flatten()[::100], 
                       alpha=0.5, s=10, c='blue')
    axes[1, 2].plot([golden_tmpImgp.min(), golden_tmpImgp.max()], 
                    [golden_tmpImgp.min(), golden_tmpImgp.max()], 
                    'r--', linewidth=2, label='Ideal')
    axes[1, 2].set_title('HLS vs Golden Scatter', fontsize=12, fontweight='bold')
    axes[1, 2].set_xlabel('Golden tmpImgp')
    axes[1, 2].set_ylabel('HLS Output')
    axes[1, 2].legend()
    axes[1, 2].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'tmpImgp_comparison.png', dpi=150, bbox_inches='tight')
    print(f"\n[Visualization] Saved: {output_dir / 'tmpImgp_comparison.png'}")
    plt.close()
    
    return rmse


def compare_and_visualize_fi(fi_result: np.ndarray, golden_aerial: np.ndarray, output_dir: Path):
    """Compare Fourier Interpolation result with Golden aerial image"""
    
    print("\n" + "=" * 70)
    print("Step 2: Fourier Interpolation vs Golden Aerial Image (1024×1024)")
    print("=" * 70)
    
    # Statistics
    print("\n[Statistics]")
    print(f"  FI Result:")
    print(f"    Range: [{fi_result.min():.6f}, {fi_result.max():.6f}]")
    print(f"    Mean:  {fi_result.mean():.6f}")
    print(f"    Std:   {fi_result.std():.6f}")
    
    print(f"  Golden Aerial:")
    print(f"    Range: [{golden_aerial.min():.6f}, {golden_aerial.max():.6f}]")
    print(f"    Mean:  {golden_aerial.mean():.6f}")
    print(f"    Std:   {golden_aerial.std():.6f}")
    
    # RMSE
    diff = fi_result - golden_aerial
    rmse = np.sqrt(np.mean(diff ** 2))
    max_diff = np.max(np.abs(diff))
    relative_error = rmse / np.mean(np.abs(golden_aerial)) * 100
    
    print(f"\n[Comparison]")
    print(f"  RMSE:            {rmse:.6e}")
    print(f"  Max Difference:  {max_diff:.6e}")
    print(f"  Relative Error:  {relative_error:.4f}%")
    
    if rmse < 1e-5:
        print(f"  Status:          ✅ PASS (RMSE < 1e-5)")
    else:
        print(f"  Status:          ⚠️  WARNING (RMSE >= 1e-5, but may be acceptable)")
    
    # Visualization
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    
    # Row 1: Heatmaps (full 1024×1024 view)
    im1 = axes[0, 0].imshow(fi_result, cmap='hot', aspect='equal')
    axes[0, 0].set_title('FI Result (Full 1024×1024)', fontsize=12, fontweight='bold')
    axes[0, 0].set_xlabel('X (pixels)')
    axes[0, 0].set_ylabel('Y (pixels)')
    plt.colorbar(im1, ax=axes[0, 0], fraction=0.046, pad=0.04)
    
    im2 = axes[0, 1].imshow(golden_aerial, cmap='hot', aspect='equal')
    axes[0, 1].set_title('Golden Aerial (Full 1024×1024)', fontsize=12, fontweight='bold')
    axes[0, 1].set_xlabel('X (pixels)')
    axes[0, 1].set_ylabel('Y (pixels)')
    plt.colorbar(im2, ax=axes[0, 1], fraction=0.046, pad=0.04)
    
    im3 = axes[0, 2].imshow(np.abs(diff), cmap='hot', aspect='equal')
    axes[0, 2].set_title(f'Absolute Difference\nMax: {max_diff:.2e}', fontsize=12, fontweight='bold')
    axes[0, 2].set_xlabel('X (pixels)')
    axes[0, 2].set_ylabel('Y (pixels)')
    plt.colorbar(im3, ax=axes[0, 2], fraction=0.046, pad=0.04)
    
    # Row 2: Profiles and histogram
    center_y = fi_result.shape[0] // 2
    axes[1, 0].plot(fi_result[center_y, 400:624], 'b-', linewidth=2, label='FI')
    axes[1, 0].plot(golden_aerial[center_y, 400:624], 'r--', linewidth=2, label='Golden')
    axes[1, 0].set_title('Center Row Profile (Y=512, X=400-624)', fontsize=12, fontweight='bold')
    axes[1, 0].set_xlabel('X (pixels)')
    axes[1, 0].set_ylabel('Intensity')
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    
    axes[1, 1].hist(fi_result.flatten(), bins=50, alpha=0.7, label='FI', color='blue')
    axes[1, 1].hist(golden_aerial.flatten(), bins=50, alpha=0.7, label='Golden', color='red')
    axes[1, 1].set_title('Intensity Distribution', fontsize=12, fontweight='bold')
    axes[1, 1].set_xlabel('Intensity')
    axes[1, 1].set_ylabel('Frequency')
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)
    
    # Scatter plot
    axes[1, 2].scatter(golden_aerial.flatten()[::1000], fi_result.flatten()[::1000], 
                       alpha=0.5, s=10, c='blue')
    axes[1, 2].plot([golden_aerial.min(), golden_aerial.max()], 
                    [golden_aerial.min(), golden_aerial.max()], 
                    'r--', linewidth=2, label='Ideal')
    axes[1, 2].set_title('FI vs Golden Scatter', fontsize=12, fontweight='bold')
    axes[1, 2].set_xlabel('Golden Aerial')
    axes[1, 2].set_ylabel('FI Result')
    axes[1, 2].legend()
    axes[1, 2].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'fi_comparison.png', dpi=150, bbox_inches='tight')
    print(f"\n[Visualization] Saved: {output_dir / 'fi_comparison.png'}")
    plt.close()
    
    return rmse


def main():
    parser = argparse.ArgumentParser(description='Complete Validation Pipeline')
    parser.add_argument('--config', type=str, 
                        default='input/config/golden_1024.json',
                        help='Configuration JSON file')
    parser.add_argument('--hls-output', type=str,
                        default='source/SOCS_HLS/data/output_nx8_golden_hls.bin',
                        help='HLS output binary file')
    parser.add_argument('--output-dir', type=str,
                        default='output/validation',
                        help='Output directory for visualizations')
    
    args = parser.parse_args()
    
    # Paths
    config_path = PROJECT_ROOT / args.config
    hls_output_path = PROJECT_ROOT / args.hls_output
    output_dir = PROJECT_ROOT / args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("=" * 70)
    print("Complete Validation Pipeline: HLS → tmpImgp → FI → Aerial Image")
    print("=" * 70)
    print(f"Config:     {config_path}")
    print(f"HLS Output: {hls_output_path}")
    print(f"Output Dir: {output_dir}")
    
    # Load configuration
    config = load_config(config_path)
    print(f"\n[Configuration]")
    print(f"  Lx, Ly:   {config['mask']['period']['Lx']}×{config['mask']['period']['Ly']}")
    print(f"  NA:       {config['optics']['NA']}")
    print(f"  Wavelength: {config['optics']['wavelength']} nm")
    print(f"  nk:       {config.get('soc', {}).get('nk', 10)}")
    
    # Load HLS output (33×33 for Nx=8)
    print(f"\n[Loading Data]")
    hls_output = load_binary_data(hls_output_path)
    print(f"  HLS Output: {len(hls_output)} pixels")
    
    # Determine output size based on data length
    if len(hls_output) == 1089:  # 33×33 (Nx=8)
        output_size = 33
        hls_output = hls_output.reshape(output_size, output_size)
        print(f"    Reshaped to {output_size}×{output_size}")
    elif len(hls_output) == 16384:  # 128×128 (full tmpImgp)
        output_size = 128
        hls_output = hls_output.reshape(output_size, output_size)
        print(f"    Reshaped to {output_size}×{output_size}")
    else:
        raise ValueError(f"Unexpected HLS output size: {len(hls_output)}")
    
    print(f"    Range: [{hls_output.min():.6f}, {hls_output.max():.6f}]")
    
    # Load Golden tmpImgp (128×128)
    golden_tmpImgp_path = PROJECT_ROOT / "output/verification/tmpImgp_full_128.bin"
    golden_tmpImgp = load_binary_data(golden_tmpImgp_path)
    golden_tmpImgp = golden_tmpImgp.reshape(128, 128)
    print(f"  Golden tmpImgp: {golden_tmpImgp.shape}, range [{golden_tmpImgp.min():.6f}, {golden_tmpImgp.max():.6f}]")
    
    # Extract corresponding region from Golden tmpImgp for comparison
    if output_size == 33:
        # Extract 33×33 region from center of 128×128 tmpImgp
        # Offset = (128 - 33) / 2 = 47
        offset = (128 - output_size) // 2
        golden_extract = golden_tmpImgp[offset:offset+output_size, offset:offset+output_size]
        print(f"  Golden Extract: {golden_extract.shape} (offset={offset})")
        
        # Compare HLS output with extracted Golden region
        rmse_tmpImgp = compare_and_visualize_tmpImgp(hls_output, golden_extract, output_dir, output_size)
    else:
        # Full 128×128 comparison
        rmse_tmpImgp = compare_and_visualize_tmpImgp(hls_output, golden_tmpImgp, output_dir, output_size)
    
    # Step 2: Perform Fourier Interpolation on FULL 128×128 tmpImgp
    print("\n" + "=" * 70)
    print("Performing Fourier Interpolation on 128×128 tmpImgp...")
    print("=" * 70)
    fi_result = fourier_interpolation_128_to_1024(golden_tmpImgp)
    
    # Load Golden aerial image (1024×1024)
    golden_aerial_path = PROJECT_ROOT / "output/verification/aerial_image_socs_kernel.bin"
    golden_aerial = load_binary_data(golden_aerial_path)
    golden_aerial = golden_aerial.reshape(1024, 1024)
    print(f"\n  Golden Aerial: {golden_aerial.shape}, range [{golden_aerial.min():.6f}, {golden_aerial.max():.6f}]")
    
    # Step 3: Compare FI result with Golden aerial
    rmse_fi = compare_and_visualize_fi(fi_result, golden_aerial, output_dir)
    
    # Final summary
    print("\n" + "=" * 70)
    print("Validation Summary")
    print("=" * 70)
    print(f"  tmpImgp RMSE:  {rmse_tmpImgp:.6e} {'✅ PASS' if rmse_tmpImgp < 1e-5 else '❌ FAIL'}")
    print(f"  FI RMSE:       {rmse_fi:.6e} {'✅ PASS' if rmse_fi < 1e-5 else '⚠️  WARNING'}")
    print(f"\n  Output files:")
    print(f"    - {output_dir / 'tmpImgp_comparison.png'}")
    print(f"    - {output_dir / 'fi_comparison.png'}")
    
    # Save FI result for further analysis
    fi_output_path = output_dir / 'fi_result_1024x1024.bin'
    fi_result.astype(np.float32).tofile(str(fi_output_path))
    print(f"    - {fi_output_path}")
    
    return 0 if rmse_tmpImgp < 1e-5 else 1


if __name__ == "__main__":
    sys.exit(main())
