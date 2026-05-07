#!/usr/bin/env python3
"""
Generate Figure 5-4: FPGA vs MATLAB Golden Visual Comparison (10 kernels)

Academic-style visualization for Chapter 5 of FPGA-Litho paper.

Output: fig_visual_comparison_10kernels.png (1024×1024 resolution)

Data sources:
  - Golden: output/verification/aerial_image_tcc_direct.bin (1024×1024)
  - FPGA: output/verification/tmpImgp_full_128.bin (128×128 → FI → 1024×1024)

Author: FPGA-Litho Project
Date: 2026-05-07
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from pathlib import Path
import matplotlib

matplotlib.use("Agg")

# Configure matplotlib for academic paper style
plt.rcParams.update({
    'font.family': 'serif',
    'font.size': 11,
    'axes.labelsize': 12,
    'axes.titlesize': 13,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 10,
    'figure.dpi': 300,
    'savefig.dpi': 300,
    'savefig.bbox': 'tight',
    'axes.grid': False,
})

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def fourier_interpolation_numpy(input_img, output_size):
    """
    Fourier Interpolation using numpy FFT.
    
    Args:
        input_img: 2D array (in_sizeX x in_sizeY)
        output_size: tuple (out_sizeX, out_sizeY)
    
    Returns:
        output_img: 2D array (out_sizeX x out_sizeY)
    """
    in_sizeX, in_sizeY = input_img.shape
    out_sizeX, out_sizeY = output_size
    
    # Step 1: Move DC from center to corner (reverse fftshift)
    shifted_in = np.fft.ifftshift(input_img)
    
    # Step 2: Forward FFT
    spectrum = np.fft.fft2(shifted_in)
    spectrum /= in_sizeX * in_sizeY  # Normalize
    
    # Step 3: Embed spectrum into larger output size
    output_spectrum = np.zeros((out_sizeY, out_sizeX), dtype=np.complex128)
    
    half_x_in = in_sizeX // 2
    half_y_in = in_sizeY // 2
    dif_x = out_sizeX - in_sizeX
    dif_y = out_sizeY - in_sizeY
    
    # Positive-positive quadrant
    for y in range(half_y_in + 1):
        for x in range(half_x_in + 1):
            output_spectrum[y, x] = spectrum[y, x]
    
    # Positive-negative quadrant (x negative)
    for y in range(half_y_in + 1):
        for x in range(half_x_in + 1, in_sizeX):
            out_x = x + dif_x
            output_spectrum[y, out_x] = spectrum[y, x]
    
    # Negative-positive quadrant (y negative)
    for y in range(half_y_in + 1, in_sizeY):
        for x in range(half_x_in + 1):
            out_y = y + dif_y
            output_spectrum[out_y, x] = spectrum[y, x]
    
    # Negative-negative quadrant
    for y in range(half_y_in + 1, in_sizeY):
        for x in range(half_x_in + 1, in_sizeX):
            out_y = y + dif_y
            out_x = x + dif_x
            output_spectrum[out_y, out_x] = spectrum[y, x]
    
    # Step 4: Inverse FFT
    output_img = np.fft.ifft2(output_spectrum).real * (out_sizeX * out_sizeY)
    
    # Step 5: Move DC back to center
    output_img = np.fft.fftshift(output_img)
    
    return output_img


def compute_metrics(golden, fpga):
    """Compute image quality metrics."""
    diff = golden - fpga
    rmse = np.sqrt(np.mean(diff ** 2))
    max_abs_error = np.max(np.abs(diff))
    psnr = 20 * np.log10(np.max(golden) / rmse) if rmse > 0 else float('inf')
    
    # Correlation coefficient
    correlation = np.corrcoef(golden.flatten(), fpga.flatten())[0, 1]
    
    return {
        'rmse': rmse,
        'max_abs_error': max_abs_error,
        'psnr': psnr,
        'correlation': correlation
    }


def main():
    print("=" * 70)
    print("Generating Figure 5-4: FPGA vs MATLAB Golden Visual Comparison")
    print("=" * 70)
    
    # ========================================================================
    # Step 1: Load data
    # ========================================================================
    print("\n[Step 1] Loading data...")
    
    # Load Golden aerial image (1024×1024)
    golden_path = PROJECT_ROOT / "output/verification/aerial_image_tcc_direct.bin"
    if not golden_path.exists():
        print(f"❌ ERROR: {golden_path} not found!")
        return
    
    golden_aerial = np.fromfile(golden_path, dtype=np.float32).reshape(1024, 1024)
    print(f"✓ Loaded Golden aerial: {golden_aerial.shape}")
    
    # Load FPGA tmpImgp (128×128)
    fpga_tmpimgp_path = PROJECT_ROOT / "output/verification/tmpImgp_full_128.bin"
    if not fpga_tmpimgp_path.exists():
        print(f"❌ ERROR: {fpga_tmpimgp_path} not found!")
        return
    
    fpga_tmpimgp = np.fromfile(fpga_tmpimgp_path, dtype=np.float32).reshape(128, 128)
    print(f"✓ Loaded FPGA tmpImgp: {fpga_tmpimgp.shape}")
    
    # ========================================================================
    # Step 2: Apply Fourier Interpolation
    # ========================================================================
    print("\n[Step 2] Applying Fourier Interpolation (128×128 → 1024×1024)...")
    
    fpga_aerial = fourier_interpolation_numpy(fpga_tmpimgp, (1024, 1024))
    print(f"✓ FPGA aerial after FI: {fpga_aerial.shape}")
    
    # ========================================================================
    # Step 3: Compute metrics
    # ========================================================================
    print("\n[Step 3] Computing image quality metrics...")
    
    metrics = compute_metrics(golden_aerial, fpga_aerial)
    print(f"  RMSE:            {metrics['rmse']:.6e}")
    print(f"  Max Abs Error:   {metrics['max_abs_error']:.6e}")
    print(f"  PSNR:            {metrics['psnr']:.2f} dB")
    print(f"  Correlation:     {metrics['correlation']:.10f}")
    
    # ========================================================================
    # Step 4: Create academic-style visualization
    # ========================================================================
    print("\n[Step 4] Creating academic-style visualization...")
    
    # Create figure with 1×3 layout
    fig = plt.figure(figsize=(15, 5))
    gs = GridSpec(1, 3, figure=fig, wspace=0.25)
    
    # Common color scale for aerial images
    vmin_aerial = min(golden_aerial.min(), fpga_aerial.min())
    vmax_aerial = max(golden_aerial.max(), fpga_aerial.max())
    
    # Left: MATLAB Golden
    ax1 = fig.add_subplot(gs[0, 0])
    im1 = ax1.imshow(golden_aerial, cmap='hot', interpolation='nearest',
                     vmin=vmin_aerial, vmax=vmax_aerial)
    ax1.set_title('(a) MATLAB Golden Aerial Image', fontweight='bold', pad=10)
    ax1.set_xlabel('X (pixels)')
    ax1.set_ylabel('Y (pixels)')
    cbar1 = plt.colorbar(im1, ax=ax1, fraction=0.046, pad=0.04)
    cbar1.set_label('Intensity', rotation=270, labelpad=15)
    
    # Middle: FPGA Output
    ax2 = fig.add_subplot(gs[0, 1])
    im2 = ax2.imshow(fpga_aerial, cmap='hot', interpolation='nearest',
                     vmin=vmin_aerial, vmax=vmax_aerial)
    ax2.set_title('(b) FPGA Output Aerial Image', fontweight='bold', pad=10)
    ax2.set_xlabel('X (pixels)')
    ax2.set_ylabel('Y (pixels)')
    cbar2 = plt.colorbar(im2, ax=ax2, fraction=0.046, pad=0.04)
    cbar2.set_label('Intensity', rotation=270, labelpad=15)
    
    # Right: Residual Map
    residual = golden_aerial - fpga_aerial
    ax3 = fig.add_subplot(gs[0, 2])
    
    # Use symmetric colormap for residual
    vmax_residual = np.percentile(np.abs(residual), 99)
    im3 = ax3.imshow(residual, cmap='RdBu_r', interpolation='nearest',
                     vmin=-vmax_residual, vmax=vmax_residual)
    ax3.set_title(f'(c) Residual Map (RMSE={metrics["rmse"]:.2e})',
                  fontweight='bold', pad=10)
    ax3.set_xlabel('X (pixels)')
    ax3.set_ylabel('Y (pixels)')
    cbar3 = plt.colorbar(im3, ax=ax3, fraction=0.046, pad=0.04)
    cbar3.set_label('Residual', rotation=270, labelpad=15)
    
    # Add overall title
    fig.suptitle('Figure 5-4: FPGA vs MATLAB Golden Visual Comparison (10 Kernels)',
                 fontsize=14, fontweight='bold', y=1.02)
    
    # ========================================================================
    # Step 5: Save figure
    # ========================================================================
    print("\n[Step 5] Saving figure...")
    
    output_path = PROJECT_ROOT / "doc/论文/fig_visual_comparison_10kernels.png"
    plt.savefig(output_path, dpi=300, bbox_inches='tight', pad_inches=0.1)
    print(f"✓ Figure saved to: {output_path}")
    
    # Also save to validation directory for reference
    output_path_ref = PROJECT_ROOT / "validation/board/jtag/fig_visual_comparison_10kernels.png"
    plt.savefig(output_path_ref, dpi=300, bbox_inches='tight', pad_inches=0.1)
    print(f"✓ Reference copy saved to: {output_path_ref}")
    
    plt.close()
    
    print("\n" + "=" * 70)
    print("✅ Figure 5-4 generation completed successfully!")
    print("=" * 70)
    
    # Print summary for paper
    print("\n📊 Summary for Chapter 5:")
    print(f"  - Image resolution: 1024×1024")
    print(f"  - RMSE: {metrics['rmse']:.6e}")
    print(f"  - Max absolute error: {metrics['max_abs_error']:.6e}")
    print(f"  - PSNR: {metrics['psnr']:.2f} dB")
    print(f"  - Correlation coefficient: {metrics['correlation']:.10f}")


if __name__ == "__main__":
    main()
