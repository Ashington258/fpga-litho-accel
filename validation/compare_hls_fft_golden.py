#!/usr/bin/env python3
"""
HLS FFT Fixed-Point vs Golden Float Comparison

Purpose: Validate HLS fixed-point FFT implementation against Golden reference

Key Understanding:
- Golden uses FFTW (float, high precision)
- HLS uses ap_fixed<16,1> (16-bit, 1 integer bit, range -1.0 to ~0.9999)
- Need to simulate HLS precision truncation for fair comparison

Data Files:
- Golden: tmpImgp_full_32.bin (32×32 float, IFFT output before FI)
- Golden: tmpImgp_pad32.bin (17×17 float, cropped to convX×convY)
- HLS: Will simulate fixed-point processing of same input
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import struct

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# ============================================================================
# Fixed-Point Simulation (matching HLS ap_fixed<16,1>)
# ============================================================================


class FixedPoint16:
    """
    Simulate HLS ap_fixed<16,1>:
    - 16 bits total
    - 1 bit integer (signed)
    - 15 bits fractional
    - Range: -1.0 to ~0.999969 (2^15 values)
    - Resolution: 2^(-15) ≈ 3.05e-5
    """

    TOTAL_BITS = 16
    INT_BITS = 1
    FRAC_BITS = 15
    SCALE = 2**FRAC_BITS  # 32768
    MIN_VAL = -1.0
    MAX_VAL = (2**TOTAL_BITS - 1) / SCALE - 1.0  # ~0.999969

    @staticmethod
    def to_fixed(val):
        """Convert float to fixed-point representation"""
        # Clamp to valid range
        val = np.clip(val, FixedPoint16.MIN_VAL, FixedPoint16.MAX_VAL)
        # Quantize to fixed-point
        fixed = np.round(val * FixedPoint16.SCALE)
        return fixed / FixedPoint16.SCALE

    @staticmethod
    def quantize_complex(c):
        """Quantize complex number to fixed-point"""
        real = FixedPoint16.to_fixed(c.real)
        imag = FixedPoint16.to_fixed(c.imag)
        return complex(real, imag)


def simulate_hls_fft_2d(input_float, is_inverse=True):
    """
    Simulate HLS 2D FFT with fixed-point precision

    Steps:
    1. Quantize input to ap_fixed<16,1>
    2. Perform FFT (row-wise then column-wise)
    3. Quantize intermediate results (FFT IP behavior)
    4. Return quantized output
    """
    # Quantize input to fixed-point (HLS input conversion)
    input_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in row] for row in input_float]
    )

    # Perform 2D FFT/IFFT using numpy (as reference FFT algorithm)
    # Row-wise FFT
    row_result = (
        np.fft.fft(input_fixed, axis=1)
        if not is_inverse
        else np.fft.ifft(input_fixed, axis=1)
    )

    # Quantize after row FFT (HLS IP intermediate precision)
    row_result_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in row] for row in row_result]
    )

    # Column-wise FFT
    col_result = (
        np.fft.fft(row_result_fixed, axis=0)
        if not is_inverse
        else np.fft.ifft(row_result_fixed, axis=0)
    )

    # Final quantization
    output_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in col] for col in col_result]
    )

    return output_fixed


def simulate_hls_scaled_fft_2d(input_float, is_inverse=True):
    """
    Simulate HLS scaled FFT (matching Vivado FFT IP scaled mode)

    Vivado FFT scaled mode divides by growth factor at each stage:
    - Forward FFT: Divides by 2 at each stage to prevent overflow
    - IFFT: Also uses scaling for fixed-point overflow prevention

    For 32-point FFT (log2(32)=5 stages):
    - Total scaling factor = 2^5 = 32 (or controlled by config)
    """
    N = input_float.shape[0]  # FFT size
    num_stages = int(np.log2(N))

    # Quantize input
    input_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in row] for row in input_float]
    )

    # Row FFT with scaling simulation
    row_result = (
        np.fft.fft(input_fixed, axis=1)
        if not is_inverse
        else np.fft.ifft(input_fixed, axis=1)
    )

    # Apply scaling (scaled mode divides by 2 per stage)
    # For scaled IFFT, output is divided by N (approximately)
    if True:  # Scaled mode
        scaling_factor = N  # Vivado scaled FFT divides by N for overflow prevention
        row_result = row_result / scaling_factor

    row_result_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in row] for row in row_result]
    )

    # Column FFT
    col_result = (
        np.fft.fft(row_result_fixed, axis=0)
        if not is_inverse
        else np.fft.ifft(row_result_fixed, axis=0)
    )

    # Apply scaling again
    if True:  # Scaled mode
        col_result = col_result / scaling_factor

    output_fixed = np.array(
        [[FixedPoint16.quantize_complex(val) for val in col] for col in col_result]
    )

    return output_fixed


# ============================================================================
# Golden Data Loading
# ============================================================================


def load_golden_tmpImgp_full():
    """Load Golden 32×32 tmpImgp (IFFT output before FI)"""
    path = PROJECT_ROOT / "output/verification/tmpImgp_full_32.bin"
    if not path.exists():
        raise FileNotFoundError(f"Golden data not found: {path}")

    data = np.fromfile(str(path), dtype=np.float32)
    if len(data) != 32 * 32:
        print(f"[WARNING] Expected 1024 values, got {len(data)}")

    return data.reshape(32, 32)


def load_golden_tmpImgp_cropped():
    """Load Golden 17×17 tmpImgp (cropped to convX×convY)"""
    path = PROJECT_ROOT / "output/verification/tmpImgp_pad32.bin"
    if not path.exists():
        raise FileNotFoundError(f"Golden data not found: {path}")

    data = np.fromfile(str(path), dtype=np.float32)
    return data.reshape(17, 17)


def load_mask_frequency():
    """Load mask frequency domain data (512×512)"""
    r_path = PROJECT_ROOT / "output/verification/mskf_r.bin"
    i_path = PROJECT_ROOT / "output/verification/mskf_i.bin"

    r = np.fromfile(str(r_path), dtype=np.float32).reshape(512, 512)
    i = np.fromfile(str(i_path), dtype=np.float32).reshape(512, 512)

    return r + 1j * i


# ============================================================================
# Comparison and Visualization
# ============================================================================


def compare_and_visualize():
    """Main comparison function"""

    print("=" * 70)
    print("HLS Fixed-Point FFT vs Golden Float Comparison")
    print("=" * 70)

    # Load Golden data
    golden_full = load_golden_tmpImgp_full()
    golden_cropped = load_golden_tmpImgp_cropped()

    print(f"\n[Golden Data Statistics]")
    print(f"  tmpImgp_full_32 (32×32):")
    print(f"    Range: [{golden_full.min():.6f}, {golden_full.max():.6f}]")
    print(f"    Mean:  {golden_full.mean():.6f}")
    print(f"    Std:   {golden_full.std():.6f}")

    print(f"  tmpImgp_pad32 (17×17):")
    print(f"    Range: [{golden_cropped.min():.6f}, {golden_cropped.max():.6f}]")
    print(f"    Mean:  {golden_cropped.mean():.6f}")
    print(f"    Std:   {golden_cropped.std():.6f}")

    # Test 1: Simulate HLS on synthetic input
    print(f"\n[Test 1] Synthetic Input - Constant value")
    synthetic_input = np.ones((32, 32), dtype=np.complex64) * 0.1
    hls_output = simulate_hls_scaled_fft_2d(synthetic_input, is_inverse=True)

    print(f"  HLS simulated output (scaled IFFT of constant 0.1):")
    print(f"    Range: [{hls_output.real.min():.6f}, {hls_output.real.max():.6f}]")
    print(f"    Mean:  {hls_output.real.mean():.6f}")

    # For scaled IFFT of constant (0.1 + 0j):
    # - FFTW unscaled: output = 0.1 * N = 3.2 (all bins)
    # - Vivado scaled: output = 0.1 / N = 0.003125 (divided by 32)
    # - HLS scaled: should match Vivado behavior

    expected_scaled = 0.1 / 32  # ~0.003125
    print(f"    Expected (scaled IFFT): ~{expected_scaled:.6f}")

    # Test 2: Try unscaled simulation (matching FFTW)
    print(f"\n[Test 2] Unscaled IFFT (FFTW behavior)")
    hls_unscaled = simulate_hls_fft_2d(synthetic_input, is_inverse=True)
    print(f"  HLS unscaled output:")
    print(f"    Range: [{hls_unscaled.real.min():.6f}, {hls_unscaled.real.max():.6f}]")
    print(f"    Mean:  {hls_unscaled.real.mean():.6f}")

    expected_unscaled = 0.1  # FFTW unscaled IFFT of constant = constant
    print(f"    Expected (unscaled IFFT): ~{expected_unscaled:.6f}")

    # Test 3: Compare with Golden data using proper input
    print(f"\n[Test 3] Real Data Comparison")

    # Golden data is the RESULT of IFFT, so we need to reverse-engineer the input
    # Or use the mask frequency data as input and compare outputs

    # The issue: Golden tmpImgp is SOCS intermediate result, not pure FFT output
    # We need to understand the full pipeline

    # Let's use Fourier Interpolation to compare
    # Apply FI to both and see if they match

    # Apply FI (upsampling) to Golden cropped (17×17)
    fi_scale = 4
    golden_fi = fourier_interpolation(golden_cropped, fi_scale)

    print(f"\n[Fourier Interpolation Comparison]")
    print(f"  Golden cropped (17×17) -> FI ({17*fi_scale}x{17*fi_scale}):")
    print(f"    Range: [{golden_fi.min():.6f}, {golden_fi.max():.6f}]")

    # Create visualization
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    fig.suptitle("HLS Fixed-Point FFT Validation vs Golden Reference", fontsize=14)

    # Row 1: Golden data
    ax1 = axes[0, 0]
    im1 = ax1.imshow(golden_full, cmap="gray")
    ax1.set_title("Golden tmpImgp_full_32 (32×32)")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")
    plt.colorbar(im1, ax=ax1, shrink=0.8)

    ax2 = axes[0, 1]
    im2 = ax2.imshow(golden_cropped, cmap="gray")
    ax2.set_title("Golden tmpImgp_pad32 (17×17)")
    ax2.set_xlabel("X")
    ax2.set_ylabel("Y")
    plt.colorbar(im2, ax=ax2, shrink=0.8)

    ax3 = axes[0, 2]
    im3 = ax3.imshow(golden_fi, cmap="gray")
    ax3.set_title(f"Golden FI {fi_scale}x ({17*fi_scale}×{17*fi_scale})")
    ax3.set_xlabel("X")
    ax3.set_ylabel("Y")
    plt.colorbar(im3, ax=ax3, shrink=0.8)

    # Row 2: HLS simulation and comparison
    ax4 = axes[1, 0]
    im4 = ax4.imshow(hls_output.real, cmap="gray")
    ax4.set_title("HLS Simulated (scaled IFFT)")
    ax4.set_xlabel("X")
    ax4.set_ylabel("Y")
    plt.colorbar(im4, ax=ax4, shrink=0.8)

    ax5 = axes[1, 1]
    im5 = ax5.imshow(hls_unscaled.real, cmap="gray")
    ax5.set_title("HLS Simulated (unscaled IFFT)")
    ax5.set_xlabel("X")
    ax5.set_ylabel("Y")
    plt.colorbar(im5, ax=ax5, shrink=0.8)

    # Cross-section comparison
    ax6 = axes[1, 2]
    center_y = 16
    ax6.plot(
        np.arange(32), golden_full[center_y, :], "b-", label="Golden (32)", linewidth=2
    )
    ax6.plot(
        np.arange(32),
        hls_unscaled.real[center_y, :],
        "r--",
        label="HLS sim",
        linewidth=1.5,
    )
    ax6.set_title("Center Row Cross-Section")
    ax6.set_xlabel("X")
    ax6.set_ylabel("Intensity")
    ax6.legend()
    ax6.grid(True, alpha=0.3)

    plt.tight_layout()

    # Save
    output_path = PROJECT_ROOT / "output/verification/hls_fft_golden_comparison.png"
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"\n[Saved] {output_path}")

    plt.show()

    # Precision analysis
    print(f"\n[Fixed-Point Precision Analysis]")
    print(f"  ap_fixed<16,1> specification:")
    print(f"    Total bits: {FixedPoint16.TOTAL_BITS}")
    print(f"    Integer bits: {FixedPoint16.INT_BITS}")
    print(f"    Fractional bits: {FixedPoint16.FRAC_BITS}")
    print(f"    Resolution: {1/FixedPoint16.SCALE:.6e}")
    print(f"    Range: [{FixedPoint16.MIN_VAL}, {FixedPoint16.MAX_VAL:.6f}]")

    # Quantization error estimate
    golden_range = golden_full.max() - golden_full.min()
    quant_error = 1 / FixedPoint16.SCALE
    relative_error = quant_error / golden_range if golden_range > 0 else quant_error

    print(f"\n  Expected quantization impact:")
    print(f"    Golden data range: {golden_range:.6f}")
    print(f"    Fixed-point resolution: {quant_error:.6e}")
    print(f"    Relative error (worst case): {relative_error:.2%}")

    print("\n" + "=" * 70)
    print("Comparison complete!")
    print("=" * 70)


def fourier_interpolation(data, scale_factor):
    """
    Fourier Interpolation: Upsample via zero-padding in frequency domain
    """
    original_size = data.shape[0]
    new_size = original_size * scale_factor

    # FFT to frequency domain
    freq_data = np.fft.fft2(data)

    # Shift DC to center
    freq_shifted = np.fft.fftshift(freq_data)

    # Zero-pad
    pad_size = (new_size - original_size) // 2
    padded_freq = np.zeros((new_size, new_size), dtype=np.complex64)
    padded_freq[
        pad_size : pad_size + original_size, pad_size : pad_size + original_size
    ] = freq_shifted

    # Shift back
    padded_freq = np.fft.ifftshift(padded_freq)

    # IFFT
    interpolated = np.fft.ifft2(padded_freq) * scale_factor

    return np.real(interpolated)


if __name__ == "__main__":
    compare_and_visualize()
