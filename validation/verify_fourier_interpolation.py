#!/usr/bin/env python3
"""
Verify Fourier Interpolation (FI) step in SOCS pipeline.

Flow: tmpImgp (17x17) → FFT → Spectrum Embedding → IFFT → aerial (512x512)

This validates Stage 4 of the SOCS pipeline (CPU post-processing).
"""

import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib

matplotlib.use("Agg")

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def my_shift(arr, shift_type_x=False, shift_type_y=False):
    """
    Match litho.cpp myShift behavior exactly.
    
    Args:
        shift_type_x: False → xh = (sizeX + 1) / 2, True → xh = sizeX / 2
        shift_type_y: False → yh = (sizeY + 1) / 2, True → yh = sizeY / 2
    
    For 32x32 (even size):
        - False: offset = 17 (litho.cpp uses (size+1)/2 for shiftType=False)
        - True: offset = 16
    """
    rows, cols = arr.shape
    xh = cols // 2 if shift_type_x else (cols + 1) // 2
    yh = rows // 2 if shift_type_y else (rows + 1) // 2
    
    # Create output array
    out = np.zeros_like(arr)
    for y in range(rows):
        for x in range(cols):
            sx = (x + xh) % cols
            sy = (y + yh) % rows
            out[sy, sx] = arr[y, x]
    return out


def fftshift_2d(arr):
    """Shift zero-frequency to center (equivalent to numpy fftshift)"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, rows // 2, axis=0), cols // 2, axis=1)


def ifftshift_2d(arr):
    """Inverse of fftshift - but DOES NOT match litho.cpp myShift for even sizes!"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, -rows // 2, axis=0), -cols // 2, axis=1)


def fourier_interpolation_numpy(input_img, output_size):
    """
    Fourier Interpolation using numpy FFT - MATCHING litho.cpp FI behavior.

    Key insight: numpy fft2 produces FULL spectrum (Nx×Ny complex), while
    FFTW R2C produces half-spectrum (Nx×(Ny/2+1) complex). This requires
    different spectrum embedding for numpy.

    Args:
        input_img: 2D array (in_sizeX x in_sizeY)
        output_size: tuple (out_sizeX, out_sizeY)

    Returns:
        output_img: 2D array (out_sizeX x out_sizeY)
    """
    in_sizeX, in_sizeY = input_img.shape
    out_sizeX, out_sizeY = output_size

    # Step 1: Use ifftshift to move zero-frequency to corner for FFT
    # tmpImgp is already fftshifted (DC at center), so ifftshift reverses it
    shifted_in = np.fft.ifftshift(input_img)

    # Step 2: Forward FFT (numpy produces full spectrum)
    spectrum = np.fft.fft2(shifted_in)
    spectrum /= in_sizeX * in_sizeY  # Normalize

    # Step 3: Embed full spectrum into larger output size
    # numpy fft2 full spectrum layout for Nx×Ny (even sizes):
    #   [0,0] = DC (lowest frequency)
    #   [0..Ny/2, 0..Nx/2] = positive-positive quadrant
    #   [0..Ny/2, Nx/2+1..Nx-1] = positive-negative quadrant (x negative)
    #   [Ny/2+1..Ny-1, 0..Nx/2] = negative-positive quadrant (y negative)
    #   [Ny/2+1..Ny-1, Nx/2+1..Nx-1] = negative-negative quadrant
    #
    # For upsampling to out_sizeX×out_sizeY:
    #   - Copy each quadrant to corresponding position in output spectrum
    #   - Positive frequencies stay at top-left region
    #   - Negative frequencies shift to bottom-right corners

    output_spectrum = np.zeros((out_sizeY, out_sizeX), dtype=np.complex128)

    half_x_in = in_sizeX // 2  # 16 for 32
    half_y_in = in_sizeY // 2  # 16 for 32
    dif_x = out_sizeX - in_sizeX  # 480 for 512-32
    dif_y = out_sizeY - in_sizeY  # 480 for 512-32

    # Positive-positive quadrant: [0..half_y, 0..half_x] → same position
    for y in range(half_y_in + 1):  # 0..16
        for x in range(half_x_in + 1):  # 0..16
            output_spectrum[y, x] = spectrum[y, x]

    # Positive-negative quadrant (x negative): [0..half_y, half_x+1..end] → shifted right
    for y in range(half_y_in + 1):  # 0..16
        for x in range(half_x_in + 1, in_sizeX):  # 17..31
            out_x = x + dif_x  # 497..511
            output_spectrum[y, out_x] = spectrum[y, x]

    # Negative-positive quadrant (y negative): [half_y+1..end, 0..half_x] → shifted down
    for y in range(half_y_in + 1, in_sizeY):  # 17..31
        for x in range(half_x_in + 1):  # 0..16
            out_y = y + dif_y  # 497..511
            output_spectrum[out_y, x] = spectrum[y, x]

    # Negative-negative quadrant: [half_y+1..end, half_x+1..end] → shifted both
    for y in range(half_y_in + 1, in_sizeY):  # 17..31
        for x in range(half_x_in + 1, in_sizeX):  # 17..31
            out_y = y + dif_y  # 497..511
            out_x = x + dif_x  # 497..511
            output_spectrum[out_y, out_x] = spectrum[y, x]

    # Step 4: Inverse FFT to get output image
    output_img = np.fft.ifft2(output_spectrum).real * (out_sizeX * out_sizeY)

    # Step 5: Use fftshift to restore zero-frequency to center
    output_img = np.fft.fftshift(output_img)

    return output_img


def main():
    print("=" * 60)
    print("Fourier Interpolation (FI) Validation")
    print("=" * 60)

    # Load full 32x32 tmpImgp (generated with --debug flag)
    # This is the CORRECT input for FI, NOT zero-padded 17x17!
    full_32_path = PROJECT_ROOT / "output/verification/tmpImgp_full_32.bin"
    
    if not full_32_path.exists():
        print(f"\n❌ ERROR: {full_32_path} not found!")
        print("   Please run: python validation/golden/run_verification.py --debug")
        return
    
    tmpimgp_32 = np.fromfile(full_32_path, dtype=np.float32).reshape(32, 32)
    
    # Also load 17x17 for reference (HLS golden output)
    tmpimgp_17 = np.fromfile(
        PROJECT_ROOT / "output/verification/tmpImgp_pad32.bin", dtype=np.float32
    ).reshape(17, 17)

    # Verify that 17x17 is the center extraction of 32x32
    offset = (32 - 17) // 2  # Should be 7
    extracted = tmpimgp_32[offset:offset+17, offset:offset+17]
    extraction_diff = np.max(np.abs(extracted - tmpimgp_17))
    
    print(f"\n[Input]")
    print(f"  Full tmpImgp (32x32): shape={tmpimgp_32.shape}, range=[{tmpimgp_32.min():.6f}, {tmpimgp_32.max():.6f}]")
    print(f"  Center tmpImgp (17x17): range=[{tmpimgp_17.min():.6f}, {tmpimgp_17.max():.6f}]")
    print(f"  Center extraction offset: ({offset}, {offset})")
    print(f"  Extraction verification: max_diff={extraction_diff:.6e} {'✅' if extraction_diff < 1e-6 else '❌'}")

    # Apply Fourier Interpolation: 32x32 → 512x512
    print(f"\n[FI Processing]")
    print(f"  Input size: 32×32")
    print(f"  Output size: 512×512")

    aerial_numpy = fourier_interpolation_numpy(tmpimgp_32, (512, 512))
    aerial_numpy = aerial_numpy.astype(np.float32)

    print(f"  Output range: [{aerial_numpy.min():.6f}, {aerial_numpy.max():.6f}]")

    # Load Golden reference (512x512)
    aerial_golden = np.fromfile(
        PROJECT_ROOT / "output/verification/aerial_image_socs_kernel.bin",
        dtype=np.float32,
    ).reshape(512, 512)

    aerial_tcc = np.fromfile(
        PROJECT_ROOT / "output/verification/aerial_image_tcc_direct.bin",
        dtype=np.float32,
    ).reshape(512, 512)

    # Compare
    diff_numpy_vs_golden = aerial_numpy - aerial_golden
    rmse_numpy = np.sqrt(np.mean(diff_numpy_vs_golden**2))
    max_diff_numpy = np.max(np.abs(diff_numpy_vs_golden))

    diff_golden_vs_tcc = aerial_golden - aerial_tcc
    rmse_golden = np.sqrt(np.mean(diff_golden_vs_tcc**2))

    print(f"\n[Comparison Results]")
    print(f"  NumPy FI vs Golden SOCS:")
    print(f"    RMSE: {rmse_numpy:.6e}")
    print(f"    Max Diff: {max_diff_numpy:.6e}")
    print(f"    Pass: {'✅' if rmse_numpy < 1e-5 else '❌'}")

    print(f"  Golden SOCS vs TCC Direct:")
    print(f"    RMSE: {rmse_golden:.6e}")

    # Visualization
    fig = plt.figure(figsize=(16, 10))

    # Row 1: Input and intermediate
    ax1 = fig.add_subplot(2, 3, 1)
    im1 = ax1.imshow(tmpimgp_17, cmap="hot", interpolation="nearest")
    ax1.set_title("tmpImgp Input (17×17)\nHLS IP Output")
    plt.colorbar(im1, ax=ax1, fraction=0.046)

    ax2 = fig.add_subplot(2, 3, 2)
    im2 = ax2.imshow(tmpimgp_32, cmap="hot", interpolation="nearest")
    ax2.set_title("tmpImgp Zero-Padded (32×32)\nFor FI Input")
    plt.colorbar(im2, ax=ax2, fraction=0.046)

    # Row 2: Outputs
    ax4 = fig.add_subplot(2, 3, 4)
    im4 = ax4.imshow(aerial_numpy, cmap="hot", interpolation="nearest")
    ax4.set_title("NumPy FI Output (512×512)")
    plt.colorbar(im4, ax=ax4, fraction=0.046)

    ax5 = fig.add_subplot(2, 3, 5)
    im5 = ax5.imshow(aerial_golden, cmap="hot", interpolation="nearest")
    ax5.set_title("Golden SOCS Output (512×512)\nFFTW FI")
    plt.colorbar(im5, ax=ax5, fraction=0.046)

    ax6 = fig.add_subplot(2, 3, 6)
    im6 = ax6.imshow(
        np.abs(diff_numpy_vs_golden),
        cmap="hot",
        interpolation="nearest",
        vmin=0,
        vmax=max_diff_numpy * 2,
    )
    ax6.set_title(f"|NumPy - Golden| Diff\nRMSE={rmse_numpy:.2e}")
    plt.colorbar(im6, ax=ax6, fraction=0.046)

    # Additional: Center region detail
    ax3 = fig.add_subplot(2, 3, 3)
    center_detail = aerial_numpy[240:272, 240:272]  # 32x32 center
    im3 = ax3.imshow(center_detail, cmap="hot", interpolation="nearest")
    ax3.set_title("FI Output Center (32×32)\nExpected to match tmpImgp")
    plt.colorbar(im3, ax=ax3, fraction=0.046)

    plt.tight_layout()
    output_file = PROJECT_ROOT / "output/verification/fi_validation.png"
    plt.savefig(str(output_file), dpi=150, bbox_inches="tight")
    print(f"\n✓ Visualization saved: {output_file}")

    # Save NumPy FI output for future reference
    np.array(aerial_numpy, dtype=np.float32).tofile(
        PROJECT_ROOT / "output/verification/aerial_fi_numpy.bin"
    )
    print(f"✓ NumPy FI output saved: output/verification/aerial_fi_numpy.bin")


if __name__ == "__main__":
    main()
