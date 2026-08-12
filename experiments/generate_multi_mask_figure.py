#!/usr/bin/env python3
"""Generate the paper figure for the ten-sample on-board validation."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
RUN_DIR = ROOT / "experiments/runs/E1_multi_mask"
GOLDEN_DIR = ROOT / "experiments/data/E1_multi_mask/golden"
OUTPUT_PATH = ROOT / "doc/image/论文/multi_mask_fpga_results.png"


def main() -> None:
    samples = [f"T{index}" for index in range(1, 11)]
    masks = [plt.imread(GOLDEN_DIR / sample / "mask.png") for sample in samples]
    aerial_images = [
        np.fromfile(RUN_DIR / sample / "fpga_aerial_fi.bin", dtype=np.float32).reshape(1024, 1024)
        for sample in samples
    ]
    color_min = min(float(image.min()) for image in aerial_images)
    color_max = max(float(image.max()) for image in aerial_images)

    figure, axes = plt.subplots(2, 10, figsize=(20, 4.25), constrained_layout=True)
    for column, (sample, mask, aerial_image) in enumerate(
        zip(samples, masks, aerial_images, strict=True)
    ):
        axes[0, column].imshow(mask, cmap="gray", vmin=0, vmax=1, interpolation="nearest")
        axes[0, column].set_title(sample, fontsize=11, pad=3)
        axes[1, column].imshow(
            aerial_image,
            cmap="jet",
            vmin=color_min,
            vmax=color_max,
            interpolation="nearest",
        )
        axes[0, column].set_axis_off()
        axes[1, column].set_axis_off()

    figure.text(0.002, 0.74, "Mask", rotation=90, va="center", fontsize=11)
    figure.text(0.002, 0.26, "Aerial image", rotation=90, va="center", fontsize=11)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(OUTPUT_PATH, dpi=300, bbox_inches="tight", pad_inches=0.02)
    plt.close(figure)
    print(OUTPUT_PATH.relative_to(ROOT))


if __name__ == "__main__":
    main()