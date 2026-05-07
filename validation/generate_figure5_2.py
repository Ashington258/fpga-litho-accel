#!/usr/bin/env python3
"""
Generate Figure 5-2: Latency Breakdown of Single SOCS Imaging Pipeline

Academic-style visualization for Chapter 5 of FPGA-Litho paper.

Output: fig_latency_breakdown.png (300 DPI, IEEE style)

Data source: Table 5-7 in Chapter 5
  - Frequency Domain Embedding: 0.67 ms
  - 2D IFFT: 9.05 ms (85.6% of total)
  - Intensity Accumulation: 0.66 ms
  - FFTshift: 0.066 ms
  - DDR Writeback: 0.066 ms
  - Total: 10.57 ms

Author: FPGA-Litho Project
Date: 2026-05-07
"""

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# ========================================================================
# Academic Style Configuration (IEEE Standard)
# ========================================================================
plt.rcParams.update({
    'font.family': 'serif',
    'font.serif': ['Times New Roman', 'DejaVu Serif', 'STSong'],
    'font.size': 10,
    'axes.labelsize': 10,
    'axes.titlesize': 11,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 9,
    'axes.linewidth': 1.0,
    'lines.linewidth': 1.5,
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
    'figure.dpi': 300,
    'savefig.dpi': 300,
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.02,
})

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def main():
    print("=" * 70)
    print("Generating Figure 5-2: Latency Breakdown")
    print("=" * 70)
    
    # ========================================================================
    # Data Preparation
    # ========================================================================
    stages = [
        'Frequency Domain\nEmbedding',
        '2D IFFT',
        'Intensity\nAccumulation',
        'FFTshift',
        'DDR Writeback'
    ]
    
    latency_ms = [0.67, 9.05, 0.66, 0.066, 0.066]
    total_latency = sum(latency_ms)
    
    # Calculate percentages
    percentages = [lat / total_latency * 100 for lat in latency_ms]
    
    print(f"\n[Data Summary]")
    print(f"  Total latency: {total_latency:.2f} ms")
    for stage, lat, pct in zip(stages, latency_ms, percentages):
        print(f"  {stage.replace(chr(10), ' '):30s}: {lat:.3f} ms ({pct:5.1f}%)")
    
    # ========================================================================
    # Create Figure (Single Column Width: 3.5 inch)
    # ========================================================================
    fig, ax = plt.subplots(figsize=(7.0, 4.5))
    
    # Color scheme (colorblind-friendly)
    colors = ['#4472C4', '#ED7D31', '#A5A5A5', '#FFC000', '#5B9BD5']
    
    # Create bar chart
    x_pos = np.arange(len(stages))
    bars = ax.bar(x_pos, latency_ms, color=colors, width=0.6, 
                   edgecolor='black', linewidth=0.8)
    
    # Add value labels on bars
    for i, (bar, lat, pct) in enumerate(zip(bars, latency_ms, percentages)):
        height = bar.get_height()
        
        # For tall bars (2D IFFT), place label inside
        if lat > 1.0:
            ax.text(bar.get_x() + bar.get_width()/2., height * 0.5,
                   f'{lat:.2f} ms\n({pct:.1f}%)',
                   ha='center', va='center',
                   fontsize=9, fontweight='bold', color='white')
        else:
            # For short bars, place label above
            ax.text(bar.get_x() + bar.get_width()/2., height + 0.15,
                   f'{lat:.3f} ms\n({pct:.1f}%)',
                   ha='center', va='bottom',
                   fontsize=8, color='black')
    
    # Configure axes
    ax.set_xticks(x_pos)
    ax.set_xticklabels(stages, rotation=0, ha='center')
    ax.set_ylabel('Latency (ms)', fontweight='bold')
    ax.set_xlabel('Pipeline Stage', fontweight='bold')
    ax.set_title('Figure 5-2: Latency Breakdown of Single SOCS Imaging (@ 250 MHz)',
                 fontsize=11, fontweight='bold', pad=10)
    
    # Set y-axis limit
    ax.set_ylim(0, max(latency_ms) * 1.2)
    
    # Add grid (very light)
    ax.yaxis.grid(True, linestyle='--', alpha=0.3, zorder=0)
    ax.set_axisbelow(True)
    
    # Add total latency annotation
    ax.text(0.98, 0.95, f'Total: {total_latency:.2f} ms',
            transform=ax.transAxes,
            ha='right', va='top',
            fontsize=10, fontweight='bold',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    # ========================================================================
    # Save Figure
    # ========================================================================
    output_path = PROJECT_ROOT / "doc/论文/fig_latency_breakdown.png"
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    print(f"\n✓ Figure saved to: {output_path}")
    
    # Also save PDF for publication
    output_pdf = PROJECT_ROOT / "doc/论文/fig_latency_breakdown.pdf"
    plt.savefig(output_pdf, format='pdf', bbox_inches='tight', facecolor='white')
    print(f"✓ PDF saved to: {output_pdf}")
    
    plt.close()
    
    print("\n" + "=" * 70)
    print("✅ Figure 5-2 generation completed successfully!")
    print("=" * 70)


if __name__ == "__main__":
    main()
