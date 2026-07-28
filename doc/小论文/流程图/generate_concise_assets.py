#!/usr/bin/env python3
"""Generate the Linux-only I01-I10 figure assets from JSON and float32 BIN files."""

import argparse
import hashlib
import json
import platform
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_float(path, shape):
    values = np.fromfile(path, dtype=np.float32)
    expected = int(np.prod(shape))
    if values.size != expected:
        raise ValueError(f"{path}: expected {expected} float32 values, got {values.size}")
    if not np.isfinite(values).all():
        raise ValueError(f"{path}: contains NaN or Inf")
    return values.reshape(shape)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def save_image(path, image, vmin, vmax, cmap, interpolation, size, dpi):
    path.parent.mkdir(parents=True, exist_ok=True)
    figure = plt.figure(figsize=(size / dpi, size / dpi), dpi=dpi, facecolor="white")
    axes = figure.add_axes([0, 0, 1, 1])
    axes.imshow(image, cmap=cmap, vmin=vmin, vmax=vmax, interpolation=interpolation)
    axes.set_axis_off()
    figure.savefig(path, dpi=dpi, facecolor="white", pad_inches=0)
    plt.close(figure)


def record(records, asset_id, path, relative_path, source, image, transform, vmin, vmax, cmap):
    records.append({
        "id": asset_id,
        "file": str(relative_path),
        "source": str(source),
        "source_type": "FPGA_BOARD" if "fpga_" in source.name else "CPU_GOLDEN",
        "dtype": "float32",
        "shape": list(image.shape),
        "display_transform": transform,
        "colormap": cmap,
        "vmin": float(vmin),
        "vmax": float(vmax),
        "sha256": sha256(path),
    })


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--golden-dir", type=Path, required=True)
    parser.add_argument("--fpga-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--image-size", type=int, default=1200)
    parser.add_argument("--dpi", type=int, default=300)
    parser.add_argument("--colormap", default="viridis")
    args = parser.parse_args()

    config = json.loads(args.config.read_text())
    if (config["mask"]["size"]["maskSizeX"], config["mask"]["size"]["maskSizeY"]) != (1024, 1024):
        raise ValueError("only the 1024x1024 figure configuration is allowed")
    if config["kernel"]["count"] != 10:
        raise ValueError("the figure workflow requires exactly 10 kernels")
    nx = int(0.8 * 1024 * (1 + 0.9) / 193)
    if nx != 8:
        raise ValueError(f"unexpected Nx={nx}; expected 8")

    golden = args.golden_dir
    fpga = args.fpga_dir
    mask_r = read_float(golden / "mskf_r.bin", (1024, 1024))
    mask_i = read_float(golden / "mskf_i.bin", (1024, 1024))
    scales = read_float(golden / "scales.bin", (10,))
    cpu_tmp = read_float(golden / "tmpImgp_full_128.bin", (128, 128))
    cpu_aerial = read_float(golden / "aerial_image_socs_kernel.bin", (1024, 1024))
    fpga_tmp = read_float(fpga / "fpga_tmpimgp_full_128.bin", (128, 128))
    fpga_aerial = read_float(fpga / "fpga_aerial_fi.bin", (1024, 1024))
    kernels = []
    for index in range(10):
        real = read_float(golden / "kernels" / f"krn_{index}_r.bin", (17, 17))
        imag = read_float(golden / "kernels" / f"krn_{index}_i.bin", (17, 17))
        kernels.append(real + 1j * imag)

    shutil.rmtree(args.output_dir, ignore_errors=True)
    args.output_dir.mkdir(parents=True)
    records = []
    cmap = args.colormap
    mask_abs = np.log1p(np.abs(mask_r + 1j * mask_i))
    kernel_abs = [np.log1p(np.abs(kernel)) for kernel in kernels]
    kernel_range = (float(min(image.min() for image in kernel_abs)), float(max(image.max() for image in kernel_abs)))
    tmpimgp_range = (float(min(cpu_tmp.min(), fpga_tmp.min())), float(max(cpu_tmp.max(), fpga_tmp.max())))
    aerial_range = (float(min(cpu_aerial.min(), fpga_aerial.min())), float(max(cpu_aerial.max(), fpga_aerial.max())))
    product = mask_r[504:521, 504:521] + 1j * mask_i[504:521, 504:521]
    product_abs = np.log1p(np.abs(product * kernels[0]))
    accumulated = np.zeros((128, 128), dtype=np.float64)
    single_kernel_intensity = None
    for index, kernel in enumerate(kernels):
        fft_input = np.zeros((128, 128), dtype=np.complex128)
        fft_input[94:111, 94:111] = product * kernel
        field = np.fft.ifft2(fft_input) * (128 * 128)
        intensity = scales[index] * np.abs(field) ** 2
        accumulated += intensity
        if index == 0:
            single_kernel_intensity = np.fft.fftshift(intensity).astype(np.float32)
    recomputed_tmp = np.fft.fftshift(accumulated).astype(np.float32)
    recompute_rmse = float(np.sqrt(np.mean((recomputed_tmp.astype(np.float64) - cpu_tmp.astype(np.float64)) ** 2)))
    if recompute_rmse >= 1e-5:
        raise ValueError(f"recomputed CPU tmpImgp RMSE {recompute_rmse:.6e} exceeds 1e-5")
    embedded = np.zeros((128, 128), dtype=np.float32)
    embedded[94:111, 94:111] = np.abs(product * kernels[0]).astype(np.float32)

    def emit(asset_id, relative, image, source, transform="linear", limits=None, interpolation="bilinear"):
        limits = limits or (float(image.min()), float(image.max()))
        path = args.output_dir / relative
        save_image(path, image, *limits, cmap, interpolation, args.image_size, args.dpi)
        record(records, asset_id, path, path.relative_to(args.output_dir), source, image, transform, *limits, cmap)

    emit("I01-mask", Path("I01_frequency_inputs/mask_spectrum.png"), mask_abs, golden / "mskf_r.bin", "log1p(abs)", interpolation="bilinear")
    (args.output_dir / "I01_frequency_inputs/weights.csv").write_text("kernel,scale\n" + "\n".join(f"K{i + 1:02d},{value:.9g}" for i, value in enumerate(scales)) + "\n")
    for index, image in enumerate(kernel_abs):
        emit(f"I01-K{index + 1:02d}", Path(f"I01_frequency_inputs/kernels/K{index + 1:02d}.png"), image, golden / "kernels" / f"krn_{index}_r.bin", "log1p(abs)", kernel_range, "nearest")
    emit("I02-window", Path("I02_frequency_product/K01_mask_window.png"), np.log1p(np.abs(product)), golden / "mskf_r.bin", "log1p(abs)", interpolation="nearest")
    emit("I02-kernel", Path("I02_frequency_product/K01_kernel.png"), kernel_abs[0], golden / "kernels/krn_0_r.bin", "log1p(abs)", kernel_range, "nearest")
    emit("I02-product", Path("I02_frequency_product/K01_product.png"), product_abs, golden / "kernels/krn_0_r.bin", "log1p(abs)", interpolation="nearest")
    emit("I03", Path("I03_single_kernel_intensity/K01_intensity.png"), single_kernel_intensity, golden / "kernels/krn_0_r.bin", interpolation="bilinear")
    emit("I04", Path("I04_cpu_weighted_sum/cpu_tmpimgp_128.png"), cpu_tmp, golden / "tmpImgp_full_128.bin", limits=tmpimgp_range, interpolation="nearest")
    emit("I05", Path("I05_cpu_aerial/cpu_aerial_1024.png"), cpu_aerial, golden / "aerial_image_socs_kernel.bin", limits=aerial_range)
    emit("I09", Path("I09_fpga_weighted_sum/fpga_tmpimgp_128.png"), fpga_tmp, fpga / "fpga_tmpimgp_full_128.bin", limits=tmpimgp_range, interpolation="nearest")
    emit("I10", Path("I10_fpga_host_fi/fpga_aerial_1024.png"), fpga_aerial, fpga / "fpga_aerial_fi.bin", limits=aerial_range)
    emit("I07", Path("I07_fixed_grid_embedding/K01_embedded_128.png"), embedded, golden / "kernels/krn_0_r.bin", interpolation="nearest")
    emit("I06-mask", Path("I06_frequency_inputs/mask_spectrum.png"), mask_abs, golden / "mskf_r.bin", "log1p(abs)", interpolation="bilinear")
    for index, image in enumerate(kernel_abs):
        emit(f"I06-K{index + 1:02d}", Path(f"I06_frequency_inputs/kernels/K{index + 1:02d}.png"), image, golden / "kernels" / f"krn_{index}_r.bin", "log1p(abs)", kernel_range, "nearest")
    shutil.copy2(args.output_dir / "I01_frequency_inputs/weights.csv", args.output_dir / "I06_frequency_inputs/weights.csv")
    (args.output_dir / "I06_frequency_inputs/shared_with_I01.json").write_text(json.dumps({"shared_with": "I01_frequency_inputs", "source": "same JSON/BIN inputs"}, indent=2) + "\n")
    emit("I08", Path("I08_single_kernel_intensity/K01_intensity.png"), single_kernel_intensity, golden / "kernels/krn_0_r.bin", interpolation="bilinear")
    (args.output_dir / "I08_single_kernel_intensity/shared_with_I03.json").write_text(json.dumps({"shared_with": "I03_single_kernel_intensity/K01_intensity.png"}, indent=2) + "\n")
    (args.output_dir / "I07_fixed_grid_embedding/region.json").write_text(json.dumps({"grid_shape": [128, 128], "valid_region_xy": [94, 94, 17, 17]}, indent=2) + "\n")
    (args.output_dir / "manifest.json").write_text(json.dumps(records, indent=2) + "\n")
    (args.output_dir / "style_manifest.json").write_text(json.dumps({"image_size": args.image_size, "dpi": args.dpi, "colormap": cmap, "background": "white", "kernel_range": kernel_range, "tmpimgp_range": tmpimgp_range, "aerial_range": aerial_range}, indent=2) + "\n")
    report = {"status": "PASS", "git_sha": __import__("subprocess").check_output(["git", "rev-parse", "HEAD"], text=True).strip(), "config_sha256": sha256(args.config), "utc": datetime.now(timezone.utc).isoformat(), "python": sys.version, "numpy": np.__version__, "matplotlib": matplotlib.__version__, "platform": platform.platform(), "kernel_count": 10, "kernel_shape": [17, 17], "rmse_recomputed_tmpimgp": recompute_rmse, "rmse_fpga_tmpimgp": float(np.sqrt(np.mean((cpu_tmp.astype(np.float64) - fpga_tmp.astype(np.float64)) ** 2))), "rmse_fpga_aerial": float(np.sqrt(np.mean((cpu_aerial.astype(np.float64) - fpga_aerial.astype(np.float64)) ** 2))), "generated_assets": len(records)}
    (args.output_dir / "generation_report.json").write_text(json.dumps(report, indent=2) + "\n")
    files = sorted(path for path in args.output_dir.rglob("*") if path.is_file() and path.name != "SHA256SUMS")
    (args.output_dir / "SHA256SUMS").write_text("\n".join(f"{sha256(path)}  {path.relative_to(args.output_dir)}" for path in files) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (KeyError, OSError, ValueError) as error:
        print(f"[FAIL] {error}", file=sys.stderr)
        raise SystemExit(1)