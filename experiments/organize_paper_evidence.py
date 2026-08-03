#!/usr/bin/env python3
"""Aggregate raw experiment runs into paper-facing CSV and JSON tables."""

from __future__ import annotations

import csv
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_timing(path: Path) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return {row["step"]: row for row in csv.DictReader(stream)}


def read_metric(path: Path) -> dict[str, str]:
    with path.open(newline="", encoding="utf-8") as stream:
        return next(csv.DictReader(stream))


def read_meta(path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, value = line.split()
        values[key] = int(value)
    return values


def seconds(timing: dict[str, dict[str, str]], step: str) -> float:
    return float(timing[step]["elapsed_seconds"])


def organize_resolution() -> None:
    data_dir = ROOT / "experiments/data/E5_resolution"
    run_root = ROOT / "experiments/runs/E5_resolution"
    rows = []

    for size in (256, 512, 1024):
        run_dir = run_root / f"{size}x{size}"
        golden_dir = data_dir / "golden" / f"{size}x{size}_nk10"
        timing = read_timing(run_dir / "timing.csv")
        metric = read_metric(run_dir / "metrics.csv")
        meta = read_meta(golden_dir / "fft_meta.txt")

        h2c_steps = [
            "pcie_h2c_write_mskf_r",
            "pcie_h2c_write_mskf_i",
            "pcie_h2c_write_scales",
            "pcie_h2c_write_krn_r",
            "pcie_h2c_write_krn_i",
            "pcie_h2c_clear_tmpImg_ddr",
            "pcie_h2c_clear_output",
        ]
        h2c_seconds = sum(seconds(timing, step) for step in h2c_steps)
        rows.append(
            {
                "resolution_x": size,
                "resolution_y": size,
                "nx": meta["Nx"],
                "ny": meta["Ny"],
                "kernel_count": 10,
                "mask_bytes": 2 * size * size * 4,
                "h2c_seconds": h2c_seconds,
                "axilite_config_seconds": seconds(timing, "hls_configure_axilite"),
                "fpga_host_observed_seconds": seconds(timing, "fpga_compute"),
                "c2h_seconds": seconds(timing, "pcie_c2h_read_tmpimgp"),
                "rmse": float(metric["rmse"]),
                "max_abs": float(metric["max_abs"]),
                "psnr_db": float(metric["psnr"]),
                "passed": metric["passed"],
                "measurement_scope": "single run; resident generated Golden; 10 ms host busy-poll interval",
                "raw_run": run_dir.relative_to(ROOT).as_posix(),
            }
        )

    output_csv = data_dir / "resolution_stage_breakdown.csv"
    with output_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "configuration": "Annular 0.6/0.9, NA=0.8, wavelength=193 nm, nk=10",
        "measurement_warning": "fpga_host_observed_seconds includes host polling quantization and is not an FPGA hardware cycle measurement",
        "resolutions": rows,
    }
    (data_dir / "resolution_summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    blocker = data_dir / "RESOLUTION_STAGE_BREAKDOWN_BLOCKER.json"
    if blocker.exists():
        blocker.unlink()
    print(f"Wrote {output_csv.relative_to(ROOT)} with {len(rows)} board runs")

    kernel_size_dir = ROOT / "experiments/data/E3_kernel_size"
    kernel_size_dir.mkdir(parents=True, exist_ok=True)
    kernel_rows = [
        {
            "nx": row["nx"],
            "ny": row["ny"],
            "kernel_width": 2 * int(row["nx"]) + 1,
            "kernel_height": 2 * int(row["ny"]) + 1,
            "resolution_x": row["resolution_x"],
            "resolution_y": row["resolution_y"],
            "fpga_host_observed_seconds": row["fpga_host_observed_seconds"],
            "rmse": row["rmse"],
            "max_abs": row["max_abs"],
            "passed": row["passed"],
            "measurement_scope": row["measurement_scope"],
            "raw_run": row["raw_run"],
        }
        for row in rows
    ]
    kernel_csv = kernel_size_dir / "kernel_size_runtime_config.csv"
    with kernel_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(kernel_rows[0]))
        writer.writeheader()
        writer.writerows(kernel_rows)
    (kernel_size_dir / "KERNEL_SIZE_COVERAGE_BLOCKER.json").write_text(
        json.dumps(
            {
                "measured_nx": [2, 4, 8],
                "current_v18_max_nx": 8,
                "missing_requested_nx": [12, 16, 24],
                "reason": "The validated V18 bitstream has MAX_KERNEL_SIZE=17, so Nx and Ny cannot exceed 8.",
                "required_action": "Build and validate a larger-kernel bitstream before collecting Nx=12/16/24 board data.",
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {kernel_csv.relative_to(ROOT)} with {len(kernel_rows)} board runs")


def record_multi_mask_coverage() -> None:
    data_dir = ROOT / "experiments/data/E1_multi_mask"
    accuracy_file = data_dir / "multi_mask_accuracy.csv"
    if not accuracy_file.exists():
        return
    with accuracy_file.open(newline="", encoding="utf-8") as stream:
        sample_count = sum(1 for _ in csv.DictReader(stream))
    blocker = data_dir / "MULTI_MASK_SAMPLE_COUNT_BLOCKER.json"
    if sample_count < 30:
        blocker.write_text(
            json.dumps(
                {
                    "measured_on_board_samples": sample_count,
                    "minimum_requested_samples": 30,
                    "remaining_samples": 30 - sample_count,
                    "reason": "The repository currently provides 10 independent ICCAD mask windows.",
                    "required_action": "Archive and measure at least 20 additional independent mask windows.",
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
    elif blocker.exists():
        blocker.unlink()


def main() -> None:
    organize_resolution()
    record_multi_mask_coverage()


if __name__ == "__main__":
    main()