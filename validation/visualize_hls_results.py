#!/usr/bin/env python3
"""
HLS Results Visualization
- HLS 32x32 spectrum (tmpImgp)
- HLS FI 512x512 aerial image
- Golden 512x512 aerial image
- Difference map
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

PROJECT_ROOT = Path("/home/ashington/fpga-litho-accel")

# Load data
hls_32 = np.fromfile(PROJECT_ROOT / "source/SOCS_HLS/hls/data/hls_output_full_32.bin", dtype=np.float32).reshape(32, 32)
hls_fi_512 = np.fromfile(PROJECT_ROOT / "output/verification/hls_fi_512x512.bin", dtype=np.float32).reshape(512, 512)
golden_512 = np.fromfile(PROJECT_ROOT / "output/verification/aerial_image_socs_kernel.bin", dtype=np.float32).reshape(512, 512)
diff_512 = np.abs(hls_fi_512 - golden_512)

# Compute statistics
print("=" * 60)
print("HLS Results Statistics")
print("=" * 60)
print(f"HLS 32x32:   range=[{hls_32.min():.6f}, {hls_32.max():.6f}] mean={hls_32.mean():.6f}")
print(f"HLS FI 512:  range=[{hls_fi_512.min():.6f}, {hls_fi_512.max():.6f}] mean={hls_fi_512.mean():.6f}")
print(f"Golden 512:  range=[{golden_512.min():.6f}, {golden_512.max():.6f}] mean={golden_512.mean():.6f}")
print(f"Diff RMSE:   {np.sqrt(np.mean(diff_512**2)):.6e}")
print(f"Diff Max:    {diff_512.max():.6e}")
print("=" * 60)

# Create visualization
fig = plt.figure(figsize=(16, 12))

# 1. HLS 32x32 spectrum
ax1 = fig.add_subplot(2, 2, 1)
im1 = ax1.imshow(hls_32, cmap='hot', interpolation='nearest')
ax1.set_title('HLS 32x32 Spectrum (tmpImgp)', fontsize=14, fontweight='bold')
ax1.set_xlabel('X (32 pixels)')
ax1.set_ylabel('Y (32 pixels)')
plt.colorbar(im1, ax=ax1, label='Intensity')
ax1.grid(False)

# Add center region marker (17x17)
center_start = (32 - 17) // 2  # 7
rect = plt.Rectangle((center_start-0.5, center_start-0.5), 17, 17, 
                       fill=False, edgecolor='cyan', linewidth=2)
ax1.add_patch(rect)
ax1.text(center_start + 8.5, center_start - 2, '17x17 Center', 
         color='cyan', fontsize=10, ha='center')

# 2. HLS FI 512x512 aerial image
ax2 = fig.add_subplot(2, 2, 2)
im2 = ax2.imshow(hls_fi_512, cmap='hot', interpolation='nearest')
ax2.set_title('HLS FI 512x512 Aerial Image', fontsize=14, fontweight='bold')
ax2.set_xlabel('X (512 pixels)')
ax2.set_ylabel('Y (512 pixels)')
plt.colorbar(im2, ax=ax2, label='Intensity')
ax2.grid(False)

# 3. Golden 512x512 aerial image
ax3 = fig.add_subplot(2, 2, 3)
im3 = ax3.imshow(golden_512, cmap='hot', interpolation='nearest')
ax3.set_title('Golden 512x512 Aerial Image (Reference)', fontsize=14, fontweight='bold')
ax3.set_xlabel('X (512 pixels)')
ax3.set_ylabel('Y (512 pixels)')
plt.colorbar(im3, ax=ax3, label='Intensity')
ax3.grid(False)

# 4. Difference map (log scale)
ax4 = fig.add_subplot(2, 2, 4)
diff_log = np.log10(diff_512 + 1e-10)  # Add small value to avoid log(0)
im4 = ax4.imshow(diff_log, cmap='viridis', interpolation='nearest')
ax4.set_title('Difference Map (log10 scale)', fontsize=14, fontweight='bold')
ax4.set_xlabel('X (512 pixels)')
ax4.set_ylabel('Y (512 pixels)')
cbar4 = plt.colorbar(im4, ax=ax4, label='log10(|Difference|)')
ax4.grid(False)

# Add RMSE annotation
rmse = np.sqrt(np.mean(diff_512**2))
ax4.text(256, 50, f'RMSE: {rmse:.2e}', color='white', fontsize=12, 
         ha='center', bbox=dict(boxstyle='round', facecolor='black', alpha=0.7))

plt.tight_layout()

# Save figure
output_path = PROJECT_ROOT / "output/verification/hls_visualization.png"
plt.savefig(output_path, dpi=150, bbox_inches='tight')
print(f"\nSaved: {output_path}")

# Also save individual images
plt.figure(figsize=(6, 5))
plt.imshow(hls_32, cmap='hot', interpolation='nearest')
plt.title('HLS 32x32 Spectrum', fontsize=12)
plt.colorbar(label='Intensity')
plt.tight_layout()
plt.savefig(PROJECT_ROOT / "output/verification/hls_32x32.png", dpi=150)
print(f"Saved: output/verification/hls_32x32.png")

plt.figure(figsize=(8, 6))
plt.imshow(hls_fi_512, cmap='hot', interpolation='nearest')
plt.title('HLS FI 512x512 Aerial Image', fontsize=12)
plt.colorbar(label='Intensity')
plt.tight_layout()
plt.savefig(PROJECT_ROOT / "output/verification/hls_fi_512x512.png", dpi=150)
print(f"Saved: output/verification/hls_fi_512x512.png")

print("\n✓ Visualization completed!")
