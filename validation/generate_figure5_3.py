#!/usr/bin/env python3
"""
Generate Figure 5-3: Speedup and Resource Utilization

Academic-style visualization for Chapter 5 of FPGA-Litho paper.

Output: fig_performance_resource.png (300 DPI, IEEE style)

Data sources:
  - Table 5-8: Speedup comparison
  - Table 5-9: Resource utilization

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
    'xtick.labelsize': 9,
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
    print("Generating Figure 5-3: Speedup and Resource Utilization")
    print("=" * 70)
    
    # ========================================================================
    # Data Preparation
    # ========================================================================
    
    # Left plot: Speedup data (Table 5-8)
    speedup_labels = [
        'MATLAB\nTCC Direct',
        'C++\nTCC Direct',
        'MATLAB\nSOCS',
        'C++\nSOCS'
    ]
    speedup_values = [45.3, 4.28, 27.1, 3.37]
    
    # Right plot: Resource utilization (Table 5-9)
    resource_labels = ['BRAM', 'DSP', 'FF', 'LUT']
    utilization_pct = [42, 2, 9, 17]
    available = [960, 1824, 433920, 216960]
    used = [int(a * u / 100) for a, u in zip(available, utilization_pct)]
    
    print(f"\n[Speedup Data]")
    for label, val in zip(speedup_labels, speedup_values):
        print(f"  {label.replace(chr(10), ' '):20s}: {val:.2f}×")
    
    print(f"\n[Resource Utilization]")
    for label, pct, use, avail in zip(resource_labels, utilization_pct, used, available):
        print(f"  {label:6s}: {pct:3d}% ({use:,} / {avail:,})")
    
    # ========================================================================
    # Create Figure (Double Column Width: 7.0 inch)
    # ========================================================================
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.0, 3.5))
    
    # --------------------------------------------------------------------
    # Left Plot: Speedup Comparison
    # --------------------------------------------------------------------
    x_pos = np.arange(len(speedup_labels))
    
    # Color scheme: distinguish MATLAB vs C++
    colors_speedup = ['#4472C4', '#5B9BD5', '#ED7D31', '#FFC000']
    
    bars1 = ax1.bar(x_pos, speedup_values, color=colors_speedup, width=0.6,
                    edgecolor='black', linewidth=0.8)
    
    # Add value labels
    for bar, val in zip(bars1, speedup_values):
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height + 1,
                f'{val:.1f}×',
                ha='center', va='bottom',
                fontsize=10, fontweight='bold')
    
    ax1.set_xticks(x_pos)
    ax1.set_xticklabels(speedup_labels, rotation=0, ha='center', fontsize=9)
    ax1.set_ylabel('Speedup (×)', fontweight='bold')
    ax1.set_title('(a) FPGA Speedup vs CPU Baselines', fontweight='bold', pad=10)
    ax1.set_ylim(0, max(speedup_values) * 1.15)
    ax1.yaxis.grid(True, linestyle='--', alpha=0.3, zorder=0)
    ax1.set_axisbelow(True)
    
    # Add horizontal line at 1× (no speedup)
    ax1.axhline(y=1, color='gray', linestyle='--', linewidth=0.8, alpha=0.5)
    
    # --------------------------------------------------------------------
    # Right Plot: Resource Utilization
    # --------------------------------------------------------------------
    x_pos2 = np.arange(len(resource_labels))
    
    # Color scheme (colorblind-friendly)
    colors_resource = ['#4472C4', '#ED7D31', '#A5A5A5', '#FFC000']
    
    bars2 = ax2.bar(x_pos2, utilization_pct, color=colors_resource, width=0.6,
                    edgecolor='black', linewidth=0.8)
    
    # Add value labels
    for bar, pct, use in zip(bars2, utilization_pct, used):
        height = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., height + 1,
                f'{pct}%\n({use:,})',
                ha='center', va='bottom',
                fontsize=9, fontweight='bold')
    
    ax2.set_xticks(x_pos2)
    ax2.set_xticklabels(resource_labels, rotation=0, ha='center', fontsize=10)
    ax2.set_ylabel('Utilization (%)', fontweight='bold')
    ax2.set_title('(b) Resource Utilization on xcku5p', fontweight='bold', pad=10)
    ax2.set_ylim(0, 100)
    ax2.yaxis.grid(True, linestyle='--', alpha=0.3, zorder=0)
    ax2.set_axisbelow(True)
    
    # Add 75% threshold line (design limit)
    ax2.axhline(y=75, color='red', linestyle='--', linewidth=1.2, alpha=0.7)
    ax2.text(3.5, 77, 'Design Limit (75%)', ha='right', va='bottom',
             fontsize=8, color='red', fontweight='bold')
    
    plt.tight_layout()
    
    # ========================================================================
    # Save Figure
    # ========================================================================
    output_path = PROJECT_ROOT / "doc/论文/fig_performance_resource.png"
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    print(f"\n✓ Figure saved to: {output_path}")
    
    # Also save PDF for publication
    output_pdf = PROJECT_ROOT / "doc/论文/fig_performance_resource.pdf"
    plt.savefig(output_pdf, format='pdf', bbox_inches='tight', facecolor='white')
    print(f"✓ PDF saved to: {output_pdf}")
    
    plt.close()
    
    print("\n" + "=" * 70)
    print("✅ Figure 5-3 generation completed successfully!")
    print("=" * 70)


if __name__ == "__main__":
    main()
