#!/usr/bin/env python3
"""End-to-end SOCS board validation: PCIe -> FPGA -> host FI -> golden compare."""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import numpy as np

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:  # pragma: no cover - visualization is optional
    plt = None

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[4]
PCIE_PYTHON_DIR = PROJECT_ROOT / "validation/board/pcie/scripts/python"
if str(PCIE_PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PCIE_PYTHON_DIR))

from pcie_config import (  # noqa: E402
    DEFAULT_CONFIG_PATH,
    GoldenData,
    expected_output_dir,
    load_golden_data,
    load_input_config,
    load_pcie_config,
    resolve_project_path,
    validate_golden_against_config,
)
from run_pcie_validation import (  # noqa: E402
    configure_hls,
    print_layout,
    read_output,
    start_and_wait,
    verify_float_array_readback,
)
from xdma import XDMADevice  # noqa: E402


@dataclass
class StepRecord:
    name: str
    elapsed_seconds: float
    status: str
    details: dict[str, Any]


@dataclass
class CompareMetrics:
    rmse: float
    max_abs: float
    max_rel: float
    mean_abs: float
    correlation: float
    psnr: float
    reference_min: float
    reference_max: float
    actual_min: float
    actual_max: float
    threshold_rmse: float
    threshold_max_abs: float
    threshold_max_rel: float
    passed: bool


class Timeline:
    def __init__(self) -> None:
        self.steps: list[StepRecord] = []

    def add(
        self,
        name: str,
        elapsed_seconds: float,
        status: str = "PASS",
        **details: Any,
    ) -> None:
        self.steps.append(
            StepRecord(
                name=name,
                elapsed_seconds=elapsed_seconds,
                status=status,
                details=details,
            )
        )


def timed_call(timeline: Timeline, name: str, func, *args, **kwargs):
    start = time.perf_counter()
    result = func(*args, **kwargs)
    timeline.add(name, time.perf_counter() - start)
    return result


def generate_golden(config_path: Path, output_dir: Path, no_clean: bool) -> None:
    cmd = [
        sys.executable,
        str(PROJECT_ROOT / "validation/golden/run_verification.py"),
        "--config",
        str(config_path.relative_to(PROJECT_ROOT) if config_path.is_relative_to(PROJECT_ROOT) else config_path),
        "--output",
        str(output_dir.relative_to(PROJECT_ROOT) if output_dir.is_relative_to(PROJECT_ROOT) else output_dir),
    ]
    if no_clean:
        cmd.append("--no-clean")

    print("[GOLDEN] " + " ".join(cmd), flush=True)
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        raise RuntimeError(f"golden generation failed with exit code {result.returncode}")


def my_shift(data: np.ndarray, shift_type_x: bool = False, shift_type_y: bool = False) -> np.ndarray:
    """Match litho.cpp myShift for row-major 2D arrays."""

    rows, cols = data.shape
    xh = cols // 2 if shift_type_x else (cols + 1) // 2
    yh = rows // 2 if shift_type_y else (rows + 1) // 2
    return np.roll(np.roll(data, xh, axis=1), yh, axis=0)


def fourier_interpolation(tmpimgp: np.ndarray, out_width: int, out_height: int) -> np.ndarray:
    """Host-side FI matching litho.cpp FFTW R2C/C2R half-spectrum embedding."""

    in_height, in_width = tmpimgp.shape
    if out_width < in_width or out_height < in_height:
        raise ValueError(
            f"FI output {out_width}x{out_height} must be >= input {in_width}x{in_height}"
        )
    if in_width & (in_width - 1) or in_height & (in_height - 1):
        raise ValueError(f"FI input must be power-of-two, got {in_width}x{in_height}")
    if out_width & (out_width - 1) or out_height & (out_height - 1):
        raise ValueError(f"FI output must be power-of-two, got {out_width}x{out_height}")

    shifted_in = my_shift(tmpimgp.astype(np.float64), False, False)
    spectrum_in = np.fft.rfft2(shifted_in) / (in_width * in_height)

    in_wt = in_width // 2 + 1
    out_wt = out_width // 2 + 1
    spectrum_out = np.zeros((out_height, out_wt), dtype=np.complex128)

    pos_rows = in_height // 2 + 1
    copy_cols = min(in_wt, out_wt)
    spectrum_out[:pos_rows, :copy_cols] = spectrum_in[:pos_rows, :copy_cols]

    neg_rows = in_height - pos_rows
    if neg_rows > 0:
        out_y_start = pos_rows + (out_height - in_height)
        out_y_end = out_y_start + neg_rows
        if out_y_end > out_height:
            raise ValueError("FI negative frequency rows exceed output spectrum")
        spectrum_out[out_y_start:out_y_end, :copy_cols] = spectrum_in[pos_rows:, :copy_cols]

    out_real = np.fft.irfft2(spectrum_out, s=(out_height, out_width)) * (
        out_width * out_height
    )
    return my_shift(out_real, False, False).astype(np.float32)


def load_float32_2d(path: Path, width: int, height: int) -> np.ndarray:
    if not path.exists():
        raise FileNotFoundError(path)
    data = np.fromfile(path, dtype=np.float32)
    expected = width * height
    if data.size != expected:
        raise ValueError(f"{path} has {data.size} floats, expected {expected}")
    return data.reshape(height, width)


def infer_tmpimgp_shape(golden: GoldenData) -> tuple[int, int]:
    """Infer FPGA tmpImgp shape from actual golden output length.

    Some legacy `fft_meta.txt` files keep `fft_conv_size_x/y` as the mathematical
    next power-of-two (for golden_1024 this can be 64), while the V18 FPGA IP is
    fixed at 128x128 and `tmpImgp_full_128.bin` contains 16384 floats.  The BIN
    length is the authoritative shape for board validation.
    """

    meta_width = golden.meta.fft_conv_size_x
    meta_height = golden.meta.fft_conv_size_y
    output_size = golden.golden_output.size
    if meta_width * meta_height == output_size:
        return meta_height, meta_width

    side = int(math.isqrt(output_size))
    if side * side == output_size:
        return side, side

    raise ValueError(
        f"cannot infer tmpImgp shape from {output_size} floats; "
        f"fft_meta says {meta_width}x{meta_height}"
    )


def compute_metrics(
    actual: np.ndarray,
    reference: np.ndarray,
    rmse_threshold: float,
    max_abs_threshold: float,
    max_rel_threshold: float,
) -> CompareMetrics:
    if actual.shape != reference.shape:
        raise ValueError(f"shape mismatch: actual={actual.shape}, reference={reference.shape}")

    diff = actual.astype(np.float64) - reference.astype(np.float64)
    abs_diff = np.abs(diff)
    rmse = float(np.sqrt(np.mean(diff * diff)))
    max_abs = float(np.max(abs_diff))
    mean_abs = float(np.mean(abs_diff))
    denom = np.maximum(np.abs(reference.astype(np.float64)), 1e-12)
    max_rel = float(np.max(abs_diff / denom))
    ref_max_abs = float(np.max(np.abs(reference)))
    psnr = float("inf") if rmse == 0 else float(20 * math.log10(max(ref_max_abs, 1e-12) / rmse))
    if np.std(actual) == 0 or np.std(reference) == 0:
        correlation = 1.0 if np.array_equal(actual, reference) else 0.0
    else:
        correlation = float(np.corrcoef(actual.reshape(-1), reference.reshape(-1))[0, 1])

    passed = (
        rmse <= rmse_threshold
        and max_abs <= max_abs_threshold
        and max_rel <= max_rel_threshold
    )
    return CompareMetrics(
        rmse=rmse,
        max_abs=max_abs,
        max_rel=max_rel,
        mean_abs=mean_abs,
        correlation=correlation,
        psnr=psnr,
        reference_min=float(np.min(reference)),
        reference_max=float(np.max(reference)),
        actual_min=float(np.min(actual)),
        actual_max=float(np.max(actual)),
        threshold_rmse=rmse_threshold,
        threshold_max_abs=max_abs_threshold,
        threshold_max_rel=max_rel_threshold,
        passed=passed,
    )


def save_metrics_csv(path: Path, rows: dict[str, CompareMetrics]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = ["name", *CompareMetrics.__dataclass_fields__.keys()]
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for name, metrics in rows.items():
            writer.writerow({"name": name, **asdict(metrics)})


def save_timing_csv(path: Path, timeline: Timeline) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["step", "status", "elapsed_seconds", "details_json"],
        )
        writer.writeheader()
        for step in timeline.steps:
            writer.writerow(
                {
                    "step": step.name,
                    "status": step.status,
                    "elapsed_seconds": f"{step.elapsed_seconds:.9f}",
                    "details_json": json.dumps(step.details, ensure_ascii=False, sort_keys=True),
                }
            )


def write_report(
    path: Path,
    args: argparse.Namespace,
    timeline: Timeline,
    metrics: dict[str, CompareMetrics],
    output_paths: dict[str, Path],
    golden: GoldenData,
    tmpimgp_shape: tuple[int, int],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    total = sum(step.elapsed_seconds for step in timeline.steps)
    lines = [
        "# 全平台 PCIe 板级验证报告",
        "",
        f"- Config: `{args.config}`",
        f"- Golden output dir: `{golden.output_dir.relative_to(PROJECT_ROOT) if golden.output_dir.is_relative_to(PROJECT_ROOT) else golden.output_dir}`",
        f"- Lx/Ly: {golden.meta.lx}×{golden.meta.ly}",
        f"- Nx/Ny: {golden.meta.nx}×{golden.meta.ny}",
        f"- kernels: {golden.meta.kernel_count} ({golden.meta.kernel_size_x}×{golden.meta.kernel_size_y})",
        f"- FPGA tmpImgp: {tmpimgp_shape[1]}×{tmpimgp_shape[0]}",
        f"- Host FI output: {golden.meta.lx}×{golden.meta.ly}",
        "",
        "## 步骤耗时",
        "",
        "| Step | Status | Time (s) | Details |",
        "| --- | --- | ---: | --- |",
    ]
    for step in timeline.steps:
        detail = json.dumps(step.details, ensure_ascii=False, sort_keys=True)
        lines.append(
            f"| {step.name} | {step.status} | {step.elapsed_seconds:.6f} | `{detail}` |"
        )
    lines.extend(["", f"Total measured time: `{total:.6f}s`", "", "## 对比结果", ""])
    lines.extend(["| Target | PASS | RMSE | Max abs | Max rel | Corr | PSNR (dB) |", "| --- | --- | ---: | ---: | ---: | ---: | ---: |"])
    for name, item in metrics.items():
        lines.append(
            f"| {name} | {'✅' if item.passed else '❌'} | {item.rmse:.10e} | "
            f"{item.max_abs:.10e} | {item.max_rel:.10e} | {item.correlation:.10f} | {item.psnr:.2f} |"
        )
    lines.extend(["", "## 输出文件", ""])
    for name, file_path in output_paths.items():
        value = file_path.relative_to(PROJECT_ROOT) if file_path.is_relative_to(PROJECT_ROOT) else file_path
        lines.append(f"- {name}: `{value}`")
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def save_visualizations(
    output_dir: Path,
    fpga_tmpimgp: np.ndarray,
    golden_tmpimgp: np.ndarray,
    fpga_aerial: np.ndarray,
    golden_socs: np.ndarray,
    golden_tcc: np.ndarray,
) -> dict[str, Path]:
    """Save matplotlib visualizations for BIN outputs.

    Visualization is intentionally not recorded in `timing.csv`, because it is a
    reporting aid rather than part of the validation runtime path.
    """

    if plt is None:
        print("[VIS] matplotlib not available; skip visualization", flush=True)
        return {}

    output_dir.mkdir(parents=True, exist_ok=True)
    paths: dict[str, Path] = {}

    tmp_diff = fpga_tmpimgp.astype(np.float64) - golden_tmpimgp.astype(np.float64)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))
    vmin = float(min(np.min(fpga_tmpimgp), np.min(golden_tmpimgp)))
    vmax = float(max(np.max(fpga_tmpimgp), np.max(golden_tmpimgp)))
    im0 = axes[0].imshow(fpga_tmpimgp, cmap="hot", vmin=vmin, vmax=vmax)
    axes[0].set_title("FPGA tmpImgp")
    plt.colorbar(im0, ax=axes[0], fraction=0.046)
    im1 = axes[1].imshow(golden_tmpimgp, cmap="hot", vmin=vmin, vmax=vmax)
    axes[1].set_title("Golden tmpImgp")
    plt.colorbar(im1, ax=axes[1], fraction=0.046)
    tmp_vmax = float(np.max(np.abs(tmp_diff))) or 1.0
    im2 = axes[2].imshow(tmp_diff, cmap="RdBu_r", vmin=-tmp_vmax, vmax=tmp_vmax)
    axes[2].set_title("tmpImgp diff")
    plt.colorbar(im2, ax=axes[2], fraction=0.046)
    for ax in axes:
        ax.set_xlabel("X")
        ax.set_ylabel("Y")
    fig.tight_layout()
    paths["tmpimgp_visual"] = output_dir / "tmpimgp_comparison.png"
    fig.savefig(paths["tmpimgp_visual"], dpi=150, bbox_inches="tight")
    plt.close(fig)

    aerial_diff = fpga_aerial.astype(np.float64) - golden_socs.astype(np.float64)
    fig, axes = plt.subplots(2, 3, figsize=(16, 9))
    vmin = float(min(np.min(fpga_aerial), np.min(golden_socs)))
    vmax = float(max(np.max(fpga_aerial), np.max(golden_socs)))
    im0 = axes[0, 0].imshow(fpga_aerial, cmap="hot", vmin=vmin, vmax=vmax)
    axes[0, 0].set_title("FPGA + Host FI")
    plt.colorbar(im0, ax=axes[0, 0], fraction=0.046)
    im1 = axes[0, 1].imshow(golden_socs, cmap="hot", vmin=vmin, vmax=vmax)
    axes[0, 1].set_title("Golden SOCS")
    plt.colorbar(im1, ax=axes[0, 1], fraction=0.046)
    diff_vmax = float(np.percentile(np.abs(aerial_diff), 99.9)) or 1.0
    im2 = axes[0, 2].imshow(aerial_diff, cmap="RdBu_r", vmin=-diff_vmax, vmax=diff_vmax)
    axes[0, 2].set_title("FPGA FI - Golden SOCS")
    plt.colorbar(im2, ax=axes[0, 2], fraction=0.046)
    im3 = axes[1, 0].imshow(golden_tcc, cmap="hot")
    axes[1, 0].set_title("Golden TCC direct")
    plt.colorbar(im3, ax=axes[1, 0], fraction=0.046)
    socs_tcc_diff = fpga_aerial.astype(np.float64) - golden_tcc.astype(np.float64)
    tcc_vmax = float(np.percentile(np.abs(socs_tcc_diff), 99.9)) or 1.0
    im4 = axes[1, 1].imshow(socs_tcc_diff, cmap="RdBu_r", vmin=-tcc_vmax, vmax=tcc_vmax)
    axes[1, 1].set_title("FPGA FI - TCC direct")
    plt.colorbar(im4, ax=axes[1, 1], fraction=0.046)
    center_y = fpga_aerial.shape[0] // 2
    center_x = fpga_aerial.shape[1] // 2
    half = min(64, center_x, center_y)
    center = fpga_aerial[center_y - half : center_y + half, center_x - half : center_x + half]
    im5 = axes[1, 2].imshow(center, cmap="hot")
    axes[1, 2].set_title("FPGA FI center crop")
    plt.colorbar(im5, ax=axes[1, 2], fraction=0.046)
    for ax in axes.ravel():
        ax.set_xlabel("X")
        ax.set_ylabel("Y")
    fig.tight_layout()
    paths["aerial_visual"] = output_dir / "aerial_comparison.png"
    fig.savefig(paths["aerial_visual"], dpi=150, bbox_inches="tight")
    plt.close(fig)

    print(f"[VIS] Saved: {paths['tmpimgp_visual']}")
    print(f"[VIS] Saved: {paths['aerial_visual']}")
    return paths


def run_fpga(args: argparse.Namespace, timeline: Timeline, golden: GoldenData) -> np.ndarray:
    pcie_config = load_pcie_config(resolve_project_path(args.pcie_config))
    validate_golden_against_config(load_input_config(resolve_project_path(args.config)), golden, pcie_config)
    print_layout(pcie_config, golden)

    device_paths = [pcie_config.devices.h2c, pcie_config.devices.c2h]
    if pcie_config.devices.register_access in {"user", "control"}:
        device_paths.append(pcie_config.devices.register)
    XDMADevice.ensure_devices_exist(device_paths)

    with XDMADevice(
        h2c_path=pcie_config.devices.h2c,
        c2h_path=pcie_config.devices.c2h,
        register_path=pcie_config.devices.register,
        register_access=pcie_config.devices.register_access,
        chunk_bytes=pcie_config.limits.dma_chunk_bytes,
    ) as dev:
        transfers = [
            ("mskf_r", pcie_config.addresses.mskf_r, golden.mskf_r),
            ("mskf_i", pcie_config.addresses.mskf_i, golden.mskf_i),
            ("scales", pcie_config.addresses.scales, golden.scales),
            ("krn_r", pcie_config.addresses.krn_r, golden.krn_r),
            ("krn_i", pcie_config.addresses.krn_i, golden.krn_i),
        ]
        for name, address, array in transfers:
            print(f"[DMA H2C] {name:7s}: writing {array.nbytes:,d} bytes", flush=True)
            start = time.perf_counter()
            dev.write_float_array(address, array)
            elapsed = time.perf_counter() - start
            mib = array.nbytes / (1024 * 1024)
            rate = mib / elapsed if elapsed > 0 else 0.0
            print(f"[DMA H2C] {name:7s}: {mib:7.2f} MiB in {elapsed:.3f}s ({rate:.2f} MiB/s)", flush=True)
            timeline.add(
                f"pcie_h2c_write_{name}",
                elapsed,
                bytes=int(array.nbytes),
                address=f"0x{address:08x}",
                mib_per_second=rate,
            )
            if not args.skip_ddr_readback:
                start = time.perf_counter()
                verify_float_array_readback(
                    dev,
                    name,
                    address,
                    array,
                    pcie_config.limits.ddr_readback_bytes,
                )
                timeline.add(
                    f"pcie_c2h_verify_{name}",
                    time.perf_counter() - start,
                    bytes=int(min(array.nbytes, pcie_config.limits.ddr_readback_bytes)),
                    address=f"0x{address:08x}",
                )

        zero = np.zeros(pcie_config.limits.output_floats, dtype=np.float32)
        for name, address in [
            ("tmpImg_ddr", pcie_config.addresses.tmpImg_ddr),
            ("output", pcie_config.addresses.output),
        ]:
            start = time.perf_counter()
            dev.write_float_array(address, zero)
            elapsed = time.perf_counter() - start
            timeline.add(
                f"pcie_h2c_clear_{name}",
                elapsed,
                bytes=int(zero.nbytes),
                address=f"0x{address:08x}",
            )
            if not args.skip_ddr_readback:
                start = time.perf_counter()
                verify_float_array_readback(
                    dev,
                    name,
                    address,
                    zero,
                    pcie_config.limits.ddr_readback_bytes,
                )
                timeline.add(
                    f"pcie_c2h_verify_clear_{name}",
                    time.perf_counter() - start,
                    bytes=int(min(zero.nbytes, pcie_config.limits.ddr_readback_bytes)),
                    address=f"0x{address:08x}",
                )

        start = time.perf_counter()
        configure_hls(dev, pcie_config, golden)
        timeline.add("hls_configure_axilite", time.perf_counter() - start)

        start = time.perf_counter()
        status = start_and_wait(dev, pcie_config)
        timeline.add(
            "fpga_compute",
            time.perf_counter() - start,
            ap_ctrl=f"0x{status:08x}",
        )

        start = time.perf_counter()
        output = read_output(dev, pcie_config)
        timeline.add(
            "pcie_c2h_read_tmpimgp",
            time.perf_counter() - start,
            bytes=int(output.nbytes),
        )

    return output


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Full platform validation: JSON -> golden -> PCIe FPGA -> host FI -> compare"
    )
    parser.add_argument("--config", default="input/config/golden_1024.json")
    parser.add_argument("--pcie-config", default=str(DEFAULT_CONFIG_PATH.relative_to(PROJECT_ROOT)))
    parser.add_argument("--golden-output", default=None)
    parser.add_argument("--output-dir", default="source/host/full_platform/output")
    parser.add_argument("--no-visualize", action="store_true", help="Skip matplotlib PNG visualization")
    parser.add_argument("--generate-golden", action="store_true")
    parser.add_argument("--no-clean", action="store_true")
    parser.add_argument("--skip-ddr-readback", action="store_true")
    parser.add_argument(
        "--tmpimgp-only",
        action="store_true",
        help="Only validate the FPGA 128x128 tmpImgp output; skip host FI and full 1024x1024 comparisons",
    )
    parser.add_argument(
        "--fast",
        action="store_true",
        help="Fast iteration mode: skip DDR readback, visualization, host FI, and full aerial comparisons",
    )
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--reuse-fpga-output", default=None, help="Use existing tmpImgp bin instead of accessing FPGA")
    parser.add_argument("--rmse-threshold", type=float, default=1e-5)
    parser.add_argument("--max-abs-threshold", type=float, default=1e-4)
    parser.add_argument("--max-rel-threshold", type=float, default=1e-2)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.fast:
        args.skip_ddr_readback = True
        args.no_visualize = True
        args.tmpimgp_only = True

    timeline = Timeline()

    config_path = resolve_project_path(args.config)
    output_dir = resolve_project_path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    golden_output_dir = expected_output_dir(
        config_path,
        resolve_project_path(args.golden_output) if args.golden_output else None,
    )

    try:
        if args.generate_golden:
            start = time.perf_counter()
            generate_golden(config_path, golden_output_dir, args.no_clean)
            timeline.add("generate_golden", time.perf_counter() - start, output=str(golden_output_dir))

        input_config = timed_call(timeline, "load_json_config", load_input_config, config_path)
        golden = timed_call(timeline, "load_golden_data", load_golden_data, golden_output_dir)
        pcie_config = load_pcie_config(resolve_project_path(args.pcie_config))
        validate_golden_against_config(input_config, golden, pcie_config)
        tmpimgp_shape = infer_tmpimgp_shape(golden)

        if args.dry_run:
            print_layout(pcie_config, golden)
            print("[DRY-RUN] Full platform inputs and layout checks passed.")
            return 0

        if args.reuse_fpga_output:
            fpga_output_path = resolve_project_path(args.reuse_fpga_output)
            start = time.perf_counter()
            fpga_tmpimgp = np.fromfile(fpga_output_path, dtype=np.float32)
            if fpga_tmpimgp.size != golden.golden_output.size:
                raise ValueError(
                    f"reuse FPGA output length {fpga_tmpimgp.size} != expected {golden.golden_output.size}"
                )
            timeline.add("load_reused_fpga_tmpimgp", time.perf_counter() - start, path=str(fpga_output_path))
        else:
            fpga_tmpimgp = run_fpga(args, timeline, golden)

        tmpimgp_path = output_dir / "fpga_tmpimgp_full_128.bin"
        start = time.perf_counter()
        fpga_tmpimgp.astype(np.float32, copy=False).tofile(tmpimgp_path)
        timeline.add("save_fpga_tmpimgp", time.perf_counter() - start, path=str(tmpimgp_path), bytes=int(fpga_tmpimgp.nbytes))

        fpga_tmpimgp_2d = fpga_tmpimgp.reshape(tmpimgp_shape)

        if args.tmpimgp_only:
            golden_tmpimgp = golden.golden_output.reshape(tmpimgp_shape)
            start = time.perf_counter()
            metrics = {
                "tmpImgp_vs_golden": compute_metrics(
                    fpga_tmpimgp_2d,
                    golden_tmpimgp,
                    args.rmse_threshold,
                    args.max_abs_threshold,
                    args.max_rel_threshold,
                ),
            }
            timeline.add("compare_tmpimgp_only", time.perf_counter() - start)

            metrics_csv = output_dir / "metrics.csv"
            timing_csv = output_dir / "timing.csv"
            report_path = output_dir / "full_platform_report.md"
            summary_json = output_dir / "summary.json"
            save_metrics_csv(metrics_csv, metrics)
            save_timing_csv(timing_csv, timeline)
            output_paths = {
                "fpga_tmpimgp": tmpimgp_path,
                "metrics_csv": metrics_csv,
                "timing_csv": timing_csv,
                "report": report_path,
            }
            write_report(report_path, args, timeline, metrics, output_paths, golden, tmpimgp_shape)
            summary_json.write_text(
                json.dumps(
                    {
                        "config": args.config,
                        "mode": "fast" if args.fast else "tmpimgp-only",
                        "golden_output_dir": str(golden.output_dir),
                        "steps": [asdict(step) for step in timeline.steps],
                        "metrics": {key: asdict(value) for key, value in metrics.items()},
                        "outputs": {key: str(value) for key, value in output_paths.items()},
                    },
                    indent=2,
                    ensure_ascii=False,
                ),
                encoding="utf-8",
            )

            print("\n[SUMMARY]")
            for key, value in metrics.items():
                print(
                    f"  {key}: {'PASS' if value.passed else 'FAIL'} "
                    f"RMSE={value.rmse:.10e}, MaxAbs={value.max_abs:.10e}, MaxRel={value.max_rel:.10e}"
                )
            print(f"  Mode: {'fast' if args.fast else 'tmpimgp-only'}")
            print(f"  Report: {report_path}")
            print(f"  Timing: {timing_csv}")
            print(f"  Metrics: {metrics_csv}")
            return 0 if metrics["tmpImgp_vs_golden"].passed else 2

        start = time.perf_counter()
        fpga_aerial = fourier_interpolation(fpga_tmpimgp_2d, golden.meta.lx, golden.meta.ly)
        timeline.add(
            "host_fi_inverse_aerial",
            time.perf_counter() - start,
            input_shape=list(fpga_tmpimgp_2d.shape),
            output_shape=list(fpga_aerial.shape),
        )

        aerial_path = output_dir / "fpga_aerial_fi.bin"
        start = time.perf_counter()
        fpga_aerial.astype(np.float32, copy=False).tofile(aerial_path)
        timeline.add("save_host_fi_aerial", time.perf_counter() - start, path=str(aerial_path), bytes=int(fpga_aerial.nbytes))

        golden_tmpimgp = golden.golden_output.reshape(tmpimgp_shape)
        golden_socs_path = golden.output_dir / "aerial_image_socs_kernel.bin"
        golden_tcc_path = golden.output_dir / "aerial_image_tcc_direct.bin"
        golden_socs = timed_call(
            timeline,
            "load_golden_socs_aerial",
            load_float32_2d,
            golden_socs_path,
            golden.meta.lx,
            golden.meta.ly,
        )
        golden_tcc = timed_call(
            timeline,
            "load_golden_tcc_aerial",
            load_float32_2d,
            golden_tcc_path,
            golden.meta.lx,
            golden.meta.ly,
        )

        start = time.perf_counter()
        metrics = {
            "tmpImgp_vs_golden": compute_metrics(
                fpga_tmpimgp_2d,
                golden_tmpimgp,
                args.rmse_threshold,
                args.max_abs_threshold,
                args.max_rel_threshold,
            ),
            "host_FI_vs_golden_SOCS": compute_metrics(
                fpga_aerial,
                golden_socs,
                args.rmse_threshold,
                args.max_abs_threshold,
                args.max_rel_threshold,
            ),
            "host_FI_vs_TCC_direct": compute_metrics(
                fpga_aerial,
                golden_tcc,
                1e-2,
                2e-2,
                5.0,
            ),
        }
        timeline.add("compare_against_golden", time.perf_counter() - start)

        metrics_csv = output_dir / "metrics.csv"
        timing_csv = output_dir / "timing.csv"
        report_path = output_dir / "full_platform_report.md"
        summary_json = output_dir / "summary.json"
        save_metrics_csv(metrics_csv, metrics)
        save_timing_csv(timing_csv, timeline)
        output_paths = {
            "fpga_tmpimgp": tmpimgp_path,
            "host_fi_aerial": aerial_path,
            "metrics_csv": metrics_csv,
            "timing_csv": timing_csv,
            "report": report_path,
        }
        if not args.no_visualize:
            output_paths.update(
                save_visualizations(
                    output_dir,
                    fpga_tmpimgp_2d,
                    golden_tmpimgp,
                    fpga_aerial,
                    golden_socs,
                    golden_tcc,
                )
            )
        write_report(report_path, args, timeline, metrics, output_paths, golden, tmpimgp_shape)
        summary_json.write_text(
            json.dumps(
                {
                    "config": args.config,
                    "golden_output_dir": str(golden.output_dir),
                    "steps": [asdict(step) for step in timeline.steps],
                    "metrics": {key: asdict(value) for key, value in metrics.items()},
                    "outputs": {key: str(value) for key, value in output_paths.items()},
                },
                indent=2,
                ensure_ascii=False,
            ),
            encoding="utf-8",
        )

        print("\n[SUMMARY]")
        for key, value in metrics.items():
            print(
                f"  {key}: {'PASS' if value.passed else 'FAIL'} "
                f"RMSE={value.rmse:.10e}, MaxAbs={value.max_abs:.10e}, MaxRel={value.max_rel:.10e}"
            )
        print(f"  Report: {report_path}")
        print(f"  Timing: {timing_csv}")
        print(f"  Metrics: {metrics_csv}")

        critical_ok = metrics["tmpImgp_vs_golden"].passed and metrics["host_FI_vs_golden_SOCS"].passed
        return 0 if critical_ok else 2
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
