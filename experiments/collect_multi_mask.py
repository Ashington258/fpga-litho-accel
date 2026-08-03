#!/usr/bin/env python3
"""Collect V18-compatible multi-mask software and on-board accuracy evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
from skimage.metrics import structural_similarity


ROOT = Path(__file__).resolve().parents[1]
CONFIG_DIR = ROOT / "experiments/config/multi_mask"
DATA_DIR = ROOT / "experiments/data/E1_multi_mask"
RUN_DIR = ROOT / "experiments/runs/E1_multi_mask"
MASK_DIR = ROOT / "input/mask/Different_mask_tests"
THRESHOLD = 0.225


def run(command: list[str], log_path: Path) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise RuntimeError(f"command failed ({result.returncode}); see {log_path.relative_to(ROOT)}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_config(sample: str) -> Path:
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    config = {
        "simulation": {
            "description": f"Paper evidence: ICCAD 2013 {sample}, 1024x1024 V18 window",
            "version": "1.0",
        },
        "mask": {
            "period": {"Lx": 1024, "Ly": 1024},
            "size": {"maskSizeX": 1024, "maskSizeY": 1024},
            "type": "Import",
            "inputFile": f"../input/mask/Different_mask_tests/{sample}.bin",
            "dose": 1.0,
        },
        "source": {
            "gridSize": 101,
            "type": "Annular",
            "annular": {"innerRadius": 0.6, "outerRadius": 0.9},
        },
        "optics": {"NA": 0.8, "wavelength": 193, "defocus": 0.2},
        "kernel": {"count": 10, "targetIntensity": THRESHOLD},
        "output": {"baseDir": "out"},
    }
    path = CONFIG_DIR / f"config_{sample}_1024_nk10.json"
    path.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


def metric_rows(path: Path) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return {row["name"]: row for row in csv.DictReader(stream)}


def timing_rows(path: Path) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return {row["step"]: row for row in csv.DictReader(stream)}


def derived_metrics(actual_path: Path, reference_path: Path) -> dict[str, float]:
    actual = np.fromfile(actual_path, dtype=np.float32).reshape(1024, 1024)
    reference = np.fromfile(reference_path, dtype=np.float32).reshape(1024, 1024)
    difference = actual.astype(np.float64) - reference.astype(np.float64)
    rmse = float(np.sqrt(np.mean(difference * difference)))
    dynamic_range = float(reference.max() - reference.min())
    nrmse = rmse / max(dynamic_range, 1e-12)
    raw_ssim = float(structural_similarity(actual, reference, data_range=max(dynamic_range, 1e-12)))
    ssim = min(1.0, max(0.0, raw_ssim))
    binary_agreement = float(np.mean((actual >= THRESHOLD) == (reference >= THRESHOLD)))
    return {"nrmse": nrmse, "ssim": ssim, "binary_agreement": binary_agreement}


def percentile(values: list[float], q: float) -> float:
    return float(np.percentile(np.asarray(values, dtype=np.float64), q))


def collect(samples: list[str], reuse_existing: bool) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    rows = []

    for sample in samples:
        png_path = MASK_DIR / f"{sample}.png"
        bin_path = MASK_DIR / f"{sample}.bin"
        if not bin_path.exists():
            run(
                [sys.executable, "tool/png_bin_converter.py", "png2bin", str(png_path), str(bin_path)],
                RUN_DIR / sample / "png_to_bin.log",
            )

        config_path = write_config(sample)
        golden_dir = DATA_DIR / "golden" / sample
        sample_run = RUN_DIR / sample
        existing_outputs = [sample_run / "metrics.csv", sample_run / "timing.csv", sample_run / "fpga_aerial_fi.bin"]
        if not reuse_existing or not all(path.exists() for path in existing_outputs):
            run(
                [
                    sys.executable,
                    "validation/golden/run_verification.py",
                    "--config",
                    str(config_path.relative_to(ROOT)),
                    "--output",
                    str(golden_dir.relative_to(ROOT)),
                    "--quiet",
                ],
                sample_run / "golden.log",
            )
            run(
                [
                    sys.executable,
                    "source/host/full_platform/scripts/python/run_full_platform_validation.py",
                    "--config",
                    str(config_path.relative_to(ROOT)),
                    "--golden-output",
                    str(golden_dir.relative_to(ROOT)),
                    "--output-dir",
                    str(sample_run.relative_to(ROOT)),
                    "--no-visualize",
                    "--skip-ddr-readback",
                ],
                sample_run / "board.log",
            )

        metrics = metric_rows(sample_run / "metrics.csv")
        timing = timing_rows(sample_run / "timing.csv")
        implementation = metrics["host_FI_vs_golden_SOCS"]
        model = metrics["host_FI_vs_TCC_direct"]
        derived = derived_metrics(
            sample_run / "fpga_aerial_fi.bin",
            golden_dir / "aerial_image_socs_kernel.bin",
        )
        h2c_seconds = sum(
            float(row["elapsed_seconds"])
            for name, row in timing.items()
            if name.startswith("pcie_h2c_")
        )
        rows.append(
            {
                "sample": sample,
                "source_png_sha256": sha256(png_path),
                "input_bin_sha256": sha256(bin_path),
                "implementation_rmse": float(implementation["rmse"]),
                "implementation_nrmse": derived["nrmse"],
                "implementation_max_abs": float(implementation["max_abs"]),
                "implementation_psnr_db": float(implementation["psnr"]),
                "implementation_ssim": derived["ssim"],
                "binary_agreement_at_0_225": derived["binary_agreement"],
                "model_rmse_vs_tcc": float(model["rmse"]),
                "model_max_abs_vs_tcc": float(model["max_abs"]),
                "h2c_seconds": h2c_seconds,
                "fpga_host_observed_seconds": float(timing["fpga_compute"]["elapsed_seconds"]),
                "c2h_seconds": float(timing["pcie_c2h_read_tmpimgp"]["elapsed_seconds"]),
                "host_fi_seconds": float(timing["host_fi_inverse_aerial"]["elapsed_seconds"]),
                "passed": implementation["passed"],
                "raw_run": sample_run.relative_to(ROOT).as_posix(),
            }
        )
        print(f"{sample}: RMSE={rows[-1]['implementation_rmse']:.3e}, SSIM={derived['ssim']:.9f}")

    output_csv = DATA_DIR / "multi_mask_accuracy.csv"
    with output_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    numeric_fields = [
        "implementation_rmse",
        "implementation_nrmse",
        "implementation_max_abs",
        "implementation_psnr_db",
        "implementation_ssim",
        "binary_agreement_at_0_225",
        "model_rmse_vs_tcc",
        "model_max_abs_vs_tcc",
    ]
    statistics = {}
    for field in numeric_fields:
        values = [float(row[field]) for row in rows]
        statistics[field] = {
            "mean": float(np.mean(values)),
            "stddev": float(np.std(values, ddof=1)) if len(values) > 1 else 0.0,
            "p95": percentile(values, 95),
            "worst": max(values) if field not in {"implementation_psnr_db", "implementation_ssim", "binary_agreement_at_0_225"} else min(values),
        }
    worst_row = max(rows, key=lambda row: float(row["implementation_rmse"]))
    summary = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "sample_count": len(rows),
        "configuration": "1024x1024 V18 window; Annular 0.6/0.9; NA=0.8; 193 nm; nk=10",
        "threshold": THRESHOLD,
        "statistics": statistics,
        "worst_implementation_rmse_sample": worst_row["sample"],
        "all_passed": all(row["passed"] == "True" for row in rows),
        "limitations": [
            "ICCAD masks are evaluated in a standardized 1024x1024 physical window, distinct from legacy Lx=2048 software runs.",
            "CD/EPE is not reported because an edge extraction and measurement convention has not been fixed.",
            "FPGA compute time is host-observed with polling and is not a hardware cycle count.",
        ],
    }
    (DATA_DIR / "multi_mask_summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    blocker = DATA_DIR / "MULTI_MASK_ACCURACY_BLOCKER.json"
    if blocker.exists():
        blocker.unlink()
    print(f"Wrote {output_csv.relative_to(ROOT)} with {len(rows)} on-board samples")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", nargs="+", default=[f"T{index}" for index in range(1, 11)])
    parser.add_argument("--reuse-existing", action="store_true")
    args = parser.parse_args()
    collect(args.samples, args.reuse_existing)


if __name__ == "__main__":
    main()