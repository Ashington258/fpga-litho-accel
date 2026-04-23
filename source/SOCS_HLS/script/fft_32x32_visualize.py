"""
FFT 32x32 Frequency Domain Visualization with Fourier Interpolation
HLS Fixed-Point FFT Output Visualization Script

This script generates visualization of the HLS FFT 32x32 output and
applies Fourier Interpolation (FI) for enhanced display.
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Configuration
FFT_SIZE = 32
FI_SCALE = 4  # Fourier Interpolation scale factor (32 -> 128)


def generate_fft_input():
    """Generate test input for FFT (simulating HLS fixed-point input)"""
    # Create a simple pattern: centered gaussian-like
    input_data = np.zeros((FFT_SIZE, FFT_SIZE), dtype=np.complex64)

    # Add a central peak (representing DC component after shift)
    cx, cy = FFT_SIZE // 2, FFT_SIZE // 2
    for y in range(FFT_SIZE):
        for x in range(FFT_SIZE):
            # Gaussian pattern centered
            dx = (x - cx) / FFT_SIZE
            dy = (y - cy) / FFT_SIZE
            val = np.exp(-(dx * dx + dy * dy) * 4)
            # Scale to fixed-point range [-1, 1]
            input_data[y, x] = complex(val * 0.5, 0.0)

    return input_data


def simulate_hls_fft_ifft(input_data):
    """Simulate HLS FFT IFFT operation (32x32 fixed-point)"""
    # Simulate FFT (forward) to get frequency domain
    freq_domain = np.fft.fft2(input_data)

    # Apply frequency domain processing (simplified)
    # In actual HLS, this would be the SOCS kernel convolution

    # Simulate IFFT (inverse) - this matches HLS output
    output_spatial = np.fft.ifft2(freq_domain)

    return freq_domain, output_spatial


def fourier_interpolation(data, scale_factor):
    """
    Fourier Interpolation (FI): Upsample via zero-padding in frequency domain

    This technique interpolates by:
    1. FFT to frequency domain
    2. Zero-pad around DC component
    3. IFFT back to spatial domain (now larger)
    """
    original_size = data.shape[0]
    new_size = original_size * scale_factor

    # FFT to frequency domain
    freq_data = np.fft.fft2(data)

    # Shift DC to center for easier padding
    freq_shifted = np.fft.fftshift(freq_data)

    # Create zero-padded frequency array
    pad_size = (new_size - original_size) // 2
    padded_freq = np.zeros((new_size, new_size), dtype=np.complex64)

    # Copy original frequency data to center of padded array
    padded_freq[
        pad_size : pad_size + original_size, pad_size : pad_size + original_size
    ] = freq_shifted

    # Shift back (DC back to corner)
    padded_freq = np.fft.ifftshift(padded_freq)

    # IFFT to get interpolated spatial data
    interpolated = np.fft.ifft2(padded_freq)

    # Scale to maintain magnitude
    interpolated = interpolated * scale_factor

    return np.real(interpolated)


def visualize_fft_output():
    """Generate complete visualization of 32x32 FFT output with FI"""

    # Generate input
    input_data = generate_fft_input()

    # Simulate HLS FFT/IFFT
    freq_domain, output_spatial = simulate_hls_fft_ifft(input_data)

    # Apply Fourier Interpolation to output
    output_fi = fourier_interpolation(np.real(output_spatial), FI_SCALE)

    # Also interpolate the frequency domain magnitude for visualization
    freq_mag = np.abs(np.fft.fftshift(freq_domain))
    freq_mag_fi = fourier_interpolation(freq_mag, FI_SCALE)

    # Create visualization
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    fig.suptitle("HLS FFT 32x32 Output with Fourier Interpolation", fontsize=14)

    # Row 1: Input and Frequency Domain
    ax1 = axes[0, 0]
    ax1.imshow(np.real(input_data), cmap="viridis")
    ax1.set_title("Input (Real, 32x32)")
    ax1.set_xlabel("X")
    ax1.set_ylabel("Y")

    ax2 = axes[0, 1]
    ax2.imshow(freq_mag, cmap="hot")
    ax2.set_title("Frequency Domain Magnitude (32x32)")
    ax2.set_xlabel("Freq X")
    ax2.set_ylabel("Freq Y")

    ax3 = axes[0, 2]
    ax3.imshow(freq_mag_fi, cmap="hot")
    ax3.set_title(f"Frequency Domain (FI {FI_SCALE}x = {32*FI_SCALE}x{32*FI_SCALE})")
    ax3.set_xlabel("Freq X")
    ax3.set_ylabel("Freq Y")

    # Row 2: Output with and without FI
    ax4 = axes[1, 0]
    output_real = np.real(output_spatial)
    ax4.imshow(output_real, cmap="gray")
    ax4.set_title("IFFT Output (Real, 32x32)")
    ax4.set_xlabel("X")
    ax4.set_ylabel("Y")
    vmin, vmax = output_real.min(), output_real.max()
    ax4.text(
        0.5,
        -0.15,
        f"Range: [{vmin:.4f}, {vmax:.4f}]",
        transform=ax4.transAxes,
        ha="center",
        fontsize=9,
    )

    ax5 = axes[1, 1]
    im = ax5.imshow(output_fi, cmap="gray")
    ax5.set_title(f"IFFT Output with FI ({32*FI_SCALE}x{32*FI_SCALE})")
    ax5.set_xlabel("X")
    ax5.set_ylabel("Y")
    vmin_fi, vmax_fi = output_fi.min(), output_fi.max()
    ax5.text(
        0.5,
        -0.15,
        f"Range: [{vmin_fi:.4f}, {vmax_fi:.4f}]",
        transform=ax5.transAxes,
        ha="center",
        fontsize=9,
    )

    # Add colorbar
    cbar = fig.colorbar(im, ax=ax5, shrink=0.8)
    cbar.set_label("Magnitude")

    # Cross-section plot
    ax6 = axes[1, 2]
    center_row = FFT_SIZE // 2
    ax6.plot(
        np.arange(FFT_SIZE),
        output_real[center_row, :],
        "b-",
        label="Original (32)",
        linewidth=2,
    )
    # Interpolated center row
    center_row_fi = FFT_SIZE * FI_SCALE // 2
    ax6.plot(
        np.linspace(0, FFT_SIZE - 1, FFT_SIZE * FI_SCALE),
        output_fi[center_row_fi, :],
        "r--",
        label=f"FI ({32*FI_SCALE})",
        linewidth=1.5,
    )
    ax6.set_title("Center Row Cross-Section")
    ax6.set_xlabel("X Position")
    ax6.set_ylabel("Magnitude")
    ax6.legend()
    ax6.grid(True, alpha=0.3)

    plt.tight_layout()

    # Save figure
    output_dir = Path("e:/fpga-litho-accel/output/verification")
    output_dir.mkdir(parents=True, exist_ok=True)
    fig_path = output_dir / "fft_32x32_fi_visualization.png"
    plt.savefig(fig_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {fig_path}")

    plt.show()

    return freq_domain, output_spatial, output_fi


def main():
    print("=" * 60)
    print("HLS FFT 32x32 Output Visualization with Fourier Interpolation")
    print("=" * 60)
    print(f"FFT Size: {FFT_SIZE}x{FFT_SIZE}")
    print(f"Fourier Interpolation Scale: {FI_SCALE}x")
    print(f"Final Output Size: {FFT_SIZE*FI_SCALE}x{FFT_SIZE*FI_SCALE}")
    print("-" * 60)

    freq_domain, output_spatial, output_fi = visualize_fft_output()

    # Print statistics
    print("\nOutput Statistics:")
    print(f"  Frequency Domain (32x32):")
    print(
        f"    Magnitude range: [{np.abs(freq_domain).min():.6f}, {np.abs(freq_domain).max():.6f}]"
    )
    print(f"  IFFT Output (32x32):")
    print(
        f"    Real range: [{np.real(output_spatial).min():.6f}, {np.real(output_spatial).max():.6f}]"
    )
    print(f"  FI Output (128x128):")
    print(f"    Range: [{output_fi.min():.6f}, {output_fi.max():.6f}]")

    print("\n" + "=" * 60)
    print("Visualization complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
