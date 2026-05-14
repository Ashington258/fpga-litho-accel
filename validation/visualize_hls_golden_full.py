#!/usr/bin/env python3
"""
Phase 1.4+ Visualization: HLS vs Golden Comparison
  - Full 128×128 tmpImgp comparison
  - Fourier Interpolation (FI) aerial image comparison

This script validates:
  1. tmpImgp (128×128): Direct SOCS output comparison
  2. FI Aerial Image (1024×1024): After Fourier Interpolation upsampling

Golden data: output/verification/tmpImgp_full_128.bin
HLS data:    source/SOCS_HLS/data/socs_output_full_128.bin
"""

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
import os

# ============================================================================
# Configuration
# ============================================================================
PROJECT_ROOT = "/home/ashington/fpga-litho-accel"
GOLDEN_DIR = os.path.join(PROJECT_ROOT, "output/verification")
HLS_DIR = os.path.join(PROJECT_ROOT, "source/SOCS_HLS/data")

# Image dimensions
TMPIMG_SIZE = 128       # Full tmpImgp size
AERIAL_SIZE = 1024      # FI output size (Lx × Ly)
Nx = 8                  # For golden_1024
CONV_X = 4 * Nx + 1    # 33 (center extraction)

# ============================================================================
# Helper: myShift (Python implementation of C++ myShift from litho.cpp)
# ============================================================================
def my_shift(data, sizeX, sizeY, shiftTypeX=False, shiftTypeY=False):
    """
    Python implementation of C++ myShift function from litho.cpp.
    
    myShift performs a circular shift on 2D data:
      shiftTypeX=False: xh = (sizeX + 1) / 2  (shifts zero-freq FROM center TO corner)
      shiftTypeX=True:  xh = sizeX / 2          (shifts zero-freq FROM corner TO center)
    
    For even sizes: both are equivalent (shift by N/2)
    
    In the C++ code:
      xh = shiftTypeX ? sizeX / 2 : (sizeX + 1) / 2;
      out[sy * sizeX + sx] = in[y * sizeX + x];  where sx=(x+xh)%sizeX, sy=(y+yh)%sizeY
    This is equivalent to: np.roll(data, shift, axis)
    """
    xh = sizeX // 2 if shiftTypeX else (sizeX + 1) // 2
    yh = sizeY // 2 if shiftTypeY else (sizeY + 1) // 2
    return np.roll(np.roll(data, xh, axis=1), yh, axis=0)


# ============================================================================
# Helper: Fourier Interpolation (Python/NumPy implementation)
# ============================================================================
def fourier_interpolation(in_img, in_size, out_size):
    """
    Fourier Interpolation: upsample via zero-padding in frequency domain.
    
    Faithfully replicates the C++ FI function in litho.cpp using FFTW:
      1. myShift(input, shiftType=false,false) -> move DC from center to corner
      2. FFTW R2C forward FFT (unnormalized) then divide by in_sizeXY
      3. Copy low-freq content to larger half-spectrum (zero-pad high freq)
      4. FFTW C2R inverse FFT (unnormalized)
      5. myShift(output, shiftType=false,false) -> move DC from corner to center
    
    Normalization convention:
      - FFTW R2C: unnormalized -> numpy rfft2: unnormalized (match)
      - FFTW C2R: unnormalized -> numpy irfft2: divides by N*M
      - Solution: multiply numpy irfft2 output by out_sizeXY
    """
    assert in_size > 0 and out_size >= in_size
    assert (in_size & (in_size - 1)) == 0, f"in_size={in_size} not power of 2"
    assert (out_size & (out_size - 1)) == 0, f"out_size={out_size} not power of 2"
    
    difY = out_size - in_size
    in_wt = in_size // 2 + 1    # Half-spectrum width (R2C format)
    out_wt = out_size // 2 + 1
    in_sizeXY = in_size * in_size
    
    # Step 1: myShift input (move zero-freq from center to corner)
    shifted_in = my_shift(in_img.astype(np.float64), in_size, in_size, False, False)
    
    # Step 2: Forward R2C FFT (numpy rfft2 = unnormalized, same as FFTW)
    # Then normalize by in_sizeXY (matches C++ code)
    spectrum_in = np.fft.rfft2(shifted_in) / in_sizeXY
    
    # Step 3: Copy low-frequency content to output spectrum (zero-pad)
    # C++ code copies rows 0..posFreqRows-1 directly, and rows posFreqRows..in_size-1
    # with difY offset. This preserves the FFTW half-spectrum layout.
    spectrum_out = np.zeros((out_size, out_wt), dtype=np.complex128)
    
    posFreqRows = in_size // 2 + 1  # Number of positive frequency rows
    copy_cols = min(in_wt, out_wt)
    
    # Copy positive frequency region (rows 0 to posFreqRows-1)
    spectrum_out[:posFreqRows, :copy_cols] = spectrum_in[:posFreqRows, :copy_cols]
    
    # Copy negative frequency region (rows posFreqRows to in_size-1) with difY offset
    neg_rows = in_size - posFreqRows
    if neg_rows > 0:
        out_y_start = posFreqRows + difY
        out_y_end = out_y_start + neg_rows
        if out_y_end <= out_size:
            spectrum_out[out_y_start:out_y_end, :copy_cols] = spectrum_in[posFreqRows:in_size, :copy_cols]
    
    # Step 4: Inverse C2R IFFT
    # numpy irfft2 normalizes by 1/(out_sizeXY), but FFTW C2R does not.
    # Multiply by out_sizeXY to match FFTW's unnormalized output.
    out_real = np.fft.irfft2(spectrum_out, s=(out_size, out_size)) * (out_size * out_size)
    
    # Step 5: myShift output back (move zero-freq from corner to center)
    shifted_out = my_shift(out_real, out_size, out_size, False, False)
    
    return shifted_out.astype(np.float32)


# ============================================================================
# Metrics
# ============================================================================
def compute_rmse(a, b):
    return np.sqrt(np.mean((a.astype(np.float64) - b.astype(np.float64))**2))

def compute_max_error(a, b):
    return np.max(np.abs(a.astype(np.float64) - b.astype(np.float64)))

def compute_correlation(a, b):
    a_f = a.flatten().astype(np.float64)
    b_f = b.flatten().astype(np.float64)
    a_centered = a_f - np.mean(a_f)
    b_centered = b_f - np.mean(b_f)
    denom = np.sqrt(np.sum(a_centered**2) * np.sum(b_centered**2))
    if denom < 1e-15:
        return 1.0
    return np.sum(a_centered * b_centered) / denom


# ============================================================================
# Main
# ============================================================================
def main():
    print("=" * 70)
    print("  Phase 1.4+ Visualization: HLS vs Golden (tmpImgp + FI)")
    print("=" * 70)
    
    # ========================================================================
    # Step 1: Load tmpImgp data (128x128)
    # ========================================================================
    print("\n[Step 1] Loading tmpImgp data (128x128)...")
    
    golden_tmpimgp_path = os.path.join(GOLDEN_DIR, "tmpImgp_full_128.bin")
    hls_tmpimgp_path = os.path.join(HLS_DIR, "socs_output_full_128.bin")
    
    if not os.path.exists(golden_tmpimgp_path):
        print(f"[ERROR] Golden tmpImgp not found: {golden_tmpimgp_path}")
        print("  Run: python3 validation/golden/run_verification.py --config input/config/golden_1024.json")
        return
    
    if not os.path.exists(hls_tmpimgp_path):
        print(f"[ERROR] HLS tmpImgp not found: {hls_tmpimgp_path}")
        print("  Run C Simulation with OUTPUT_MODE=1 first")
        return
    
    golden_tmpimgp = np.fromfile(golden_tmpimgp_path, dtype=np.float32).reshape(TMPIMG_SIZE, TMPIMG_SIZE)
    hls_tmpimgp = np.fromfile(hls_tmpimgp_path, dtype=np.float32).reshape(TMPIMG_SIZE, TMPIMG_SIZE)
    
    print(f"  Golden: shape={golden_tmpimgp.shape}, range=[{golden_tmpimgp.min():.6f}, {golden_tmpimgp.max():.6f}]")
    print(f"  HLS:    shape={hls_tmpimgp.shape}, range=[{hls_tmpimgp.min():.6f}, {hls_tmpimgp.max():.6f}]")
    
    # tmpImgp metrics
    rmse_tmpimgp = compute_rmse(golden_tmpimgp, hls_tmpimgp)
    max_err_tmpimgp = compute_max_error(golden_tmpimgp, hls_tmpimgp)
    corr_tmpimgp = compute_correlation(golden_tmpimgp, hls_tmpimgp)
    diff_tmpimgp = np.abs(golden_tmpimgp.astype(np.float64) - hls_tmpimgp.astype(np.float64))
    
    print(f"\n  [tmpImgp 128x128 Metrics]")
    print(f"  RMSE:       {rmse_tmpimgp:.2e}")
    print(f"  Max Error:  {max_err_tmpimgp:.2e}")
    print(f"  Correlation:{corr_tmpimgp:.8f}")
    
    # ========================================================================
    # Step 2: Also load 33x33 center extraction for reference
    # ========================================================================
    print("\n[Step 2] Loading center extraction (33x33) for reference...")
    
    golden_center_path = os.path.join(GOLDEN_DIR, "tmpImgp_pad128.bin")
    if os.path.exists(golden_center_path):
        golden_center = np.fromfile(golden_center_path, dtype=np.float32).reshape(CONV_X, CONV_X)
        print(f"  Golden center: shape={golden_center.shape}, range=[{golden_center.min():.6f}, {golden_center.max():.6f}]")
    
    # Extract center from full tmpImgp for comparison
    offset = (TMPIMG_SIZE - CONV_X) // 2  # (128-33)/2 = 47
    golden_center_extracted = golden_tmpimgp[offset:offset+CONV_X, offset:offset+CONV_X]
    hls_center_extracted = hls_tmpimgp[offset:offset+CONV_X, offset:offset+CONV_X]
    rmse_center = compute_rmse(golden_center_extracted, hls_center_extracted)
    print(f"  Center RMSE (from full 128x128): {rmse_center:.2e}")
    
    # ========================================================================
    # Step 3: Fourier Interpolation - 128x128 -> 1024x1024
    # ========================================================================
    print("\n[Step 3] Running Fourier Interpolation (128x128 -> 1024x1024)...")
    
    print("  Computing FI for Golden tmpImgp...")
    golden_fi = fourier_interpolation(golden_tmpimgp, TMPIMG_SIZE, AERIAL_SIZE)
    print(f"  Golden FI: shape={golden_fi.shape}, range=[{golden_fi.min():.6f}, {golden_fi.max():.6f}]")
    
    print("  Computing FI for HLS tmpImgp...")
    hls_fi = fourier_interpolation(hls_tmpimgp, TMPIMG_SIZE, AERIAL_SIZE)
    print(f"  HLS FI:    shape={hls_fi.shape}, range=[{hls_fi.min():.6f}, {hls_fi.max():.6f}]")
    
    # FI metrics
    rmse_fi = compute_rmse(golden_fi, hls_fi)
    max_err_fi = compute_max_error(golden_fi, hls_fi)
    corr_fi = compute_correlation(golden_fi, hls_fi)
    diff_fi = np.abs(golden_fi.astype(np.float64) - hls_fi.astype(np.float64))
    
    print(f"\n  [FI Aerial Image 1024x1024 Metrics]")
    print(f"  RMSE:       {rmse_fi:.2e}")
    print(f"  Max Error:  {max_err_fi:.2e}")
    print(f"  Correlation:{corr_fi:.8f}")
    
    # ========================================================================
    # Step 4: Load reference aerial images
    # ========================================================================
    print("\n[Step 4] Loading reference aerial images...")
    
    # Load TCC direct and SOCS kernel reference images
    tcc_direct_path = os.path.join(GOLDEN_DIR, "aerial_image_tcc_direct.bin")
    socs_kernel_path = os.path.join(GOLDEN_DIR, "aerial_image_socs_kernel.bin")
    
    ref_images = {}
    if os.path.exists(tcc_direct_path):
        ref_images['TCC Direct'] = np.fromfile(tcc_direct_path, dtype=np.float32).reshape(AERIAL_SIZE, AERIAL_SIZE)
        print(f"  TCC Direct: range=[{ref_images['TCC Direct'].min():.6f}, {ref_images['TCC Direct'].max():.6f}]")
    if os.path.exists(socs_kernel_path):
        ref_images['SOCS Kernel'] = np.fromfile(socs_kernel_path, dtype=np.float32).reshape(AERIAL_SIZE, AERIAL_SIZE)
        print(f"  SOCS Kernel: range=[{ref_images['SOCS Kernel'].min():.6f}, {ref_images['SOCS Kernel'].max():.6f}]")
    
    # ========================================================================
    # Step 5: Create Visualization
    # ========================================================================
    print("\n[Step 5] Creating visualization...")
    
    fig = plt.figure(figsize=(24, 18))
    gs = GridSpec(3, 4, figure=fig, hspace=0.35, wspace=0.25)
    
    # --- Row 1: tmpImgp (128x128) Comparison ---
    ax1 = fig.add_subplot(gs[0, 0])
    im1 = ax1.imshow(golden_tmpimgp, cmap='hot', interpolation='nearest')
    ax1.set_title(f'Golden tmpImgp (128x128)\nRange: [{golden_tmpimgp.min():.4f}, {golden_tmpimgp.max():.4f}]', fontsize=10)
    ax1.set_xlabel('X')
    ax1.set_ylabel('Y')
    plt.colorbar(im1, ax=ax1, fraction=0.046, pad=0.04)
    
    ax2 = fig.add_subplot(gs[0, 1])
    im2 = ax2.imshow(hls_tmpimgp, cmap='hot', interpolation='nearest')
    ax2.set_title(f'HLS tmpImgp (128x128)\nRange: [{hls_tmpimgp.min():.4f}, {hls_tmpimgp.max():.4f}]', fontsize=10)
    ax2.set_xlabel('X')
    ax2.set_ylabel('Y')
    plt.colorbar(im2, ax=ax2, fraction=0.046, pad=0.04)
    
    ax3 = fig.add_subplot(gs[0, 2])
    im3 = ax3.imshow(diff_tmpimgp, cmap='inferno', interpolation='nearest')
    ax3.set_title(f'|Difference| tmpImgp\nRMSE={rmse_tmpimgp:.2e}, Max={max_err_tmpimgp:.2e}\nCorr={corr_tmpimgp:.8f}', fontsize=10)
    ax3.set_xlabel('X')
    ax3.set_ylabel('Y')
    plt.colorbar(im3, ax=ax3, fraction=0.046, pad=0.04)
    
    ax4 = fig.add_subplot(gs[0, 3])
    center_display = np.zeros((CONV_X, CONV_X, 3))
    gc_norm = (golden_center_extracted - golden_center_extracted.min()) / (golden_center_extracted.max() - golden_center_extracted.min() + 1e-10)
    hc_norm = (hls_center_extracted - hls_center_extracted.min()) / (hls_center_extracted.max() - hls_center_extracted.min() + 1e-10)
    center_display[:,:,0] = gc_norm
    center_display[:,:,1] = hc_norm
    center_display[:,:,2] = gc_norm * 0.5 + hc_norm * 0.5
    ax4.imshow(center_display)
    ax4.set_title(f'Center (33x33) Overlay\nR=Golden G=HLS\nRMSE={rmse_center:.2e}', fontsize=10)
    ax4.set_xlabel('X')
    ax4.set_ylabel('Y')
    
    # --- Row 2: FI Aerial Image (1024x1024) Comparison ---
    ax5 = fig.add_subplot(gs[1, 0])
    im5 = ax5.imshow(golden_fi, cmap='hot', interpolation='nearest')
    ax5.set_title(f'Golden FI Aerial (1024x1024)\nRange: [{golden_fi.min():.4f}, {golden_fi.max():.4f}]', fontsize=10)
    ax5.set_xlabel('X')
    ax5.set_ylabel('Y')
    plt.colorbar(im5, ax=ax5, fraction=0.046, pad=0.04)
    
    ax6 = fig.add_subplot(gs[1, 1])
    im6 = ax6.imshow(hls_fi, cmap='hot', interpolation='nearest')
    ax6.set_title(f'HLS FI Aerial (1024x1024)\nRange: [{hls_fi.min():.4f}, {hls_fi.max():.4f}]', fontsize=10)
    ax6.set_xlabel('X')
    ax6.set_ylabel('Y')
    plt.colorbar(im6, ax=ax6, fraction=0.046, pad=0.04)
    
    ax7 = fig.add_subplot(gs[1, 2])
    im7 = ax7.imshow(diff_fi, cmap='inferno', interpolation='nearest')
    ax7.set_title(f'|Difference| FI Aerial\nRMSE={rmse_fi:.2e}, Max={max_err_fi:.2e}\nCorr={corr_fi:.8f}', fontsize=10)
    ax7.set_xlabel('X')
    ax7.set_ylabel('Y')
    plt.colorbar(im7, ax=ax7, fraction=0.046, pad=0.04)
    
    ax8 = fig.add_subplot(gs[1, 3])
    if 'SOCS Kernel' in ref_images:
        rmse_fi_vs_socs = compute_rmse(golden_fi, ref_images['SOCS Kernel'])
        corr_fi_vs_socs = compute_correlation(golden_fi, ref_images['SOCS Kernel'])
        fi_ref_diff = np.abs(golden_fi.astype(np.float64) - ref_images['SOCS Kernel'].astype(np.float64))
        im8 = ax8.imshow(fi_ref_diff, cmap='inferno', interpolation='nearest')
        ax8.set_title(f'|Golden-FI - SOCS Ref|\nRMSE={rmse_fi_vs_socs:.2e}\nCorr={corr_fi_vs_socs:.8f}', fontsize=10)
        plt.colorbar(im8, ax=ax8, fraction=0.046, pad=0.04)
    else:
        center_line = hls_fi[AERIAL_SIZE//2, :]
        ax8.plot(center_line, 'b-', linewidth=1, label='HLS FI center line')
        golden_line = golden_fi[AERIAL_SIZE//2, :]
        ax8.plot(golden_line, 'r--', linewidth=1, label='Golden FI center line')
        ax8.legend(fontsize=8)
        ax8.set_title('FI Cross-Section (Y=512)', fontsize=10)
        ax8.set_xlabel('X')
        ax8.set_ylabel('Intensity')
    ax8.set_xlabel('X')
    ax8.set_ylabel('Y')
    
    # --- Row 3: Cross-section and Error Analysis ---
    ax9 = fig.add_subplot(gs[2, 0])
    x_axis = np.arange(TMPIMG_SIZE)
    ax9.plot(x_axis, golden_tmpimgp[TMPIMG_SIZE//2, :], 'r-', linewidth=1.5, label='Golden')
    ax9.plot(x_axis, hls_tmpimgp[TMPIMG_SIZE//2, :], 'b--', linewidth=1.5, label='HLS')
    ax9.legend(fontsize=9)
    ax9.set_title('tmpImgp Cross-Section (Y=64)', fontsize=10)
    ax9.set_xlabel('X')
    ax9.set_ylabel('Intensity')
    ax9.grid(True, alpha=0.3)
    
    ax10 = fig.add_subplot(gs[2, 1])
    x_axis_fi = np.arange(AERIAL_SIZE)
    ax10.plot(x_axis_fi, golden_fi[AERIAL_SIZE//2, :], 'r-', linewidth=1.5, label='Golden FI')
    ax10.plot(x_axis_fi, hls_fi[AERIAL_SIZE//2, :], 'b--', linewidth=1.5, label='HLS FI')
    ax10.legend(fontsize=9)
    ax10.set_title('FI Cross-Section (Y=512)', fontsize=10)
    ax10.set_xlabel('X')
    ax10.set_ylabel('Intensity')
    ax10.grid(True, alpha=0.3)
    
    ax11 = fig.add_subplot(gs[2, 2])
    errors_flat = diff_tmpimgp.flatten()
    ax11.hist(errors_flat, bins=50, color='steelblue', edgecolor='black', alpha=0.7)
    ax11.axvline(rmse_tmpimgp, color='red', linestyle='--', linewidth=1.5, label=f'RMSE={rmse_tmpimgp:.2e}')
    ax11.legend(fontsize=9)
    ax11.set_title('tmpImgp Error Distribution', fontsize=10)
    ax11.set_xlabel('|Error|')
    ax11.set_ylabel('Count')
    
    ax12 = fig.add_subplot(gs[2, 3])
    fi_errors_flat = diff_fi.flatten()
    ax12.hist(fi_errors_flat, bins=50, color='darkorange', edgecolor='black', alpha=0.7)
    ax12.axvline(rmse_fi, color='red', linestyle='--', linewidth=1.5, label=f'RMSE={rmse_fi:.2e}')
    ax12.legend(fontsize=9)
    ax12.set_title('FI Error Distribution', fontsize=10)
    ax12.set_xlabel('|Error|')
    ax12.set_ylabel('Count')
    
    fig.suptitle(
        f'Phase 1.4+ Validation: HLS vs Golden (golden_1024, Nx={Nx})\n'
        f'tmpImgp RMSE={rmse_tmpimgp:.2e} | FI RMSE={rmse_fi:.2e} | '
        f'tmpImgp Corr={corr_tmpimgp:.6f} | FI Corr={corr_fi:.6f}',
        fontsize=14, fontweight='bold', y=0.98
    )
    
    output_path = os.path.join(PROJECT_ROOT, "validation", "hls_vs_golden_phase1_4_plus.png")
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"\n  Saved: {output_path}")
    plt.close()
    
    # ========================================================================
    # Summary
    # ========================================================================
    print("\n" + "=" * 70)
    print("  VALIDATION SUMMARY")
    print("=" * 70)
    
    tmpimgp_pass = rmse_tmpimgp < 1e-5
    fi_pass = rmse_fi < 1e-3
    
    print(f"\n  tmpImgp (128x128):")
    print(f"    RMSE:       {rmse_tmpimgp:.2e}  {'PASS' if tmpimgp_pass else 'FAIL'} (< 1e-5)")
    print(f"    Max Error:  {max_err_tmpimgp:.2e}")
    print(f"    Correlation:{corr_tmpimgp:.8f}")
    
    print(f"\n  FI Aerial Image (1024x1024):")
    print(f"    RMSE:       {rmse_fi:.2e}  {'PASS' if fi_pass else 'FAIL'} (< 1e-3)")
    print(f"    Max Error:  {max_err_fi:.2e}")
    print(f"    Correlation:{corr_fi:.8f}")
    
    if 'SOCS Kernel' in ref_images:
        rmse_fi_socs = compute_rmse(golden_fi, ref_images['SOCS Kernel'])
        corr_fi_socs = compute_correlation(golden_fi, ref_images['SOCS Kernel'])
        print(f"\n  Golden-FI vs SOCS Reference:")
        print(f"    RMSE:       {rmse_fi_socs:.2e}")
        print(f"    Correlation:{corr_fi_socs:.8f}")
        print(f"    (Validates FI implementation correctness)")
    
    if 'TCC Direct' in ref_images:
        rmse_fi_tcc = compute_rmse(golden_fi, ref_images['TCC Direct'])
        corr_fi_tcc = compute_correlation(golden_fi, ref_images['TCC Direct'])
        print(f"\n  Golden-FI vs TCC Direct Reference:")
        print(f"    RMSE:       {rmse_fi_tcc:.2e}")
        print(f"    Correlation:{corr_fi_tcc:.8f}")
    
    print("\n" + "=" * 70)
    
    # Save FI outputs
    golden_fi_path = os.path.join(GOLDEN_DIR, "tmpImgp_fi_golden_1024.bin")
    hls_fi_path = os.path.join(HLS_DIR, "tmpImgp_fi_hls_1024.bin")
    golden_fi.tofile(golden_fi_path)
    hls_fi.tofile(hls_fi_path)
    print(f"  Saved Golden FI: {golden_fi_path}")
    print(f"  Saved HLS FI:    {hls_fi_path}")


if __name__ == "__main__":
    main()
