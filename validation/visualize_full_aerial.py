#!/usr/bin/env python3
"""
Visualize complete SOCS aerial image pipeline:
  - HLS IP output: 17x17 tmpImgp (convolution result)
  - Final aerial: 512x512 after Fourier Interpolation
"""

import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib

matplotlib.use("Agg")

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def main():
    # Load HLS IP output (17x17)
    hls_output = np.fromfile(
        PROJECT_ROOT / "source/SOCS_HLS/data/hls_output.bin", dtype=np.float32
    ).reshape(17, 17)

    # Load Golden tmpImgp (17x17)
    golden_tmpimgp = np.fromfile(
        PROJECT_ROOT / "source/SOCS_HLS/data/tmpImgp_pad32.bin", dtype=np.float32
    ).reshape(17, 17)

    # Load final aerial image (512x512)
    aerial_512 = np.fromfile(
        PROJECT_ROOT / "output/verification/aerial_image_socs_kernel.bin",
        dtype=np.float32,
    ).reshape(512, 512)

    # Also load TCC direct for reference
    aerial_tcc = np.fromfile(
        PROJECT_ROOT / "output/verification/aerial_image_tcc_direct.bin",
        dtype=np.float32,
    ).reshape(512, 512)

    print("=" * 60)
    print("SOCS Full Pipeline Visualization")
    print("=" * 60)
    print(f"HLS IP Output (tmpImgp): 17×17 = {hls_output.size}")
    print(f"Final Aerial (SOCS):    512×512 = {aerial_512.size}")
    print(f"TCC Direct Reference:   512×512 = {aerial_tcc.size}")

    # Create comprehensive visualization
    fig = plt.figure(figsize=(16, 12))

    # Row 1: HLS outputs
    ax1 = fig.add_subplot(2, 3, 1)
    im1 = ax1.imshow(hls_output, cmap="hot", interpolation="nearest")
    ax1.set_title("HLS IP Output (17×17 tmpImgp)\nSOCS Convolution Result")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")
    plt.colorbar(im1, ax=ax1, fraction=0.046)

    ax2 = fig.add_subplot(2, 3, 2)
    im2 = ax2.imshow(golden_tmpimgp, cmap="hot", interpolation="nearest")
    ax2.set_title("Golden tmpImgp_pad32 (17×17)\nCPU Reference")
    ax2.set_xlabel("X")
    ax2.set_ylabel("Y")
    plt.colorbar(im2, ax=ax2, fraction=0.046)

    ax3 = fig.add_subplot(2, 3, 3)
    diff_hls = hls_output - golden_tmpimgp
    im3 = ax3.imshow(
        diff_hls, cmap="RdBu", interpolation="nearest", vmin=-1e-6, vmax=1e-6
    )
    ax3.set_title("HLS - Golden Difference\n(scale: ±1e-6)")
    ax3.set_xlabel("X")
    ax3.set_ylabel("Y")
    plt.colorbar(im3, ax=ax3, fraction=0.046)

    # Row 2: Final aerial images (512x512)
    ax4 = fig.add_subplot(2, 3, 4)
    im4 = ax4.imshow(aerial_512, cmap="hot", interpolation="nearest")
    ax4.set_title("Final Aerial Image (512×512)\nSOCS + Fourier Interpolation")
    ax4.set_xlabel("X")
    ax4.set_ylabel("Y")
    plt.colorbar(im4, ax=ax4, fraction=0.046)

    ax5 = fig.add_subplot(2, 3, 5)
    im5 = ax5.imshow(aerial_tcc, cmap="hot", interpolation="nearest")
    ax5.set_title("TCC Direct Reference (512×512)\nTheoretical Standard")
    ax5.set_xlabel("X")
    ax5.set_ylabel("Y")
    plt.colorbar(im5, ax=ax5, fraction=0.046)

    ax6 = fig.add_subplot(2, 3, 6)
    diff_aerial = aerial_512 - aerial_tcc
    im6 = ax6.imshow(diff_aerial, cmap="RdBu", interpolation="nearest")
    ax6.set_title("SOCS - TCC Difference\n(512×512)")
    ax6.set_xlabel("X")
    ax6.set_ylabel("Y")
    plt.colorbar(im6, ax=ax6, fraction=0.046)

    plt.tight_layout()
    output_file = PROJECT_ROOT / "output/verification/full_aerial_pipeline.png"
    plt.savefig(str(output_file), dpi=150, bbox_inches="tight")
    print(f"\n✓ Saved: {output_file}")

    # Print statistics
    print("\n[Statistics]")
    print(f"  HLS tmpImgp range: [{hls_output.min():.6f}, {hls_output.max():.6f}]")
    print(f"  Final aerial range: [{aerial_512.min():.6f}, {aerial_512.max():.6f}]")
    print(f"  TCC direct range:   [{aerial_tcc.min():.6f}, {aerial_tcc.max():.6f}]")
    print(
        f"  HLS-Golden RMSE:    {np.sqrt(np.mean((hls_output - golden_tmpimgp)**2)):.6e}"
    )
    print(f"  SOCS-TCC RMSE:      {np.sqrt(np.mean((aerial_512 - aerial_tcc)**2)):.6e}")


if __name__ == "__main__":
    main()
