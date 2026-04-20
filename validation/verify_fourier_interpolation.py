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


def fftshift_2d(arr):
    """Shift zero-frequency to center (equivalent to numpy fftshift)"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, rows // 2, axis=0), cols // 2, axis=1)


def ifftshift_2d(arr):
    """Inverse of fftshift (move zero-frequency back to corner)"""
    rows, cols = arr.shape
    return np.roll(np.roll(arr, -rows // 2, axis=0), -cols // 2, axis=1)


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

    # Step 1: ifftshift to move zero-frequency to corner for FFT
    shifted_in = ifftshift_2d(input_img)

    # Step 2: Forward FFT (R2C equivalent using numpy)
    spectrum = np.fft.fft2(shifted_in)
    spectrum /= in_sizeX * in_sizeY  # Normalize

    # Step 3: Embed spectrum into larger output size
    # Copy low-frequency content (center region)
    in_wt = in_sizeX // 2 + 1  # Width of input half-spectrum
    out_wt = out_sizeX // 2 + 1  # Width of output half-spectrum

    # Create output spectrum (zeros)
    output_spectrum = np.zeros((out_sizeY, out_sizeX), dtype=np.complex128)

    # Copy positive frequency region (top half, including DC)
    posFreqRows = in_sizeY // 2 + 1
    for y in range(posFreqRows):
        for x in range(min(in_wt, out_wt)):
            output_spectrum[y, x] = spectrum[y, x]

    # Copy negative frequency region (bottom half) with offset
    difY = out_sizeY - in_sizeY
    for y in range(posFreqRows, in_sizeY):
        out_y = y + difY
        for x in range(min(in_wt, out_wt)):
            if out_y < out_sizeY:
                output_spectrum[out_y, x] = spectrum[y, x]

    # Step 4: Inverse FFT to get output image
    output_img = np.fft.ifft2(output_spectrum).real * (out_sizeX * out_sizeY)

    # Step 5: fftshift to restore zero-frequency to center
    output_img = fftshift_2d(output_img)

    return output_img


def main():
    print("=" * 60)
    print("Fourier Interpolation (FI) Validation")
    print("=" * 60)

    # Load tmpImgp (HLS IP output, 17x17)
    # Note: This is already extracted center region from 32x32
    tmpimgp_17 = np.fromfile(
        PROJECT_ROOT / "source/SOCS_HLS/data/tmpImgp_pad32.bin", dtype=np.float32
    ).reshape(17, 17)

    # We need the full 32x32 tmpImgp for FI
    # Let's reconstruct it by zero-padding 17x17 to 32x32 center
    tmpimgp_32 = np.zeros((32, 32), dtype=np.float64)
    offset = (32 - 17) // 2
    tmpimgp_32[offset : offset + 17, offset : offset + 17] = tmpimgp_17

    print(f"\n[Input]")
    print(
        f"  tmpImgp (17x17): shape={tmpimgp_17.shape}, range=[{tmpimgp_17.min():.6f}, {tmpimgp_17.max():.6f}]"
    )
    print(
        f"  Reconstructed tmpImgp (32x32): range=[{tmpimgp_32.min():.6f}, {tmpimgp_32.max():.6f}]"
    )

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
