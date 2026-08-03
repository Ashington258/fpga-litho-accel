#!/usr/bin/env python3
"""Collect software and on-board evidence for supported optical configurations."""

from __future__ import annotations

import csv
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG_DIR = ROOT / "experiments/config/E4_optical_config"
DATA_DIR = ROOT / "experiments/data/E4_optical_config"
RUN_DIR = ROOT / "experiments/runs/E4_optical_config"


CASES = {
    "annular_baseline": {
        "source": {"gridSize": 101, "type": "Annular", "annular": {"innerRadius": 0.6, "outerRadius": 0.9}},
        "optics": {"NA": 0.8, "wavelength": 193, "defocus": 0.2},
    },
    "dipole_x": {
        "source": {"gridSize": 101, "type": "Dipole", "dipole": {"radius": 0.2, "offset": 0.6, "onXAxis": True}},
        "optics": {"NA": 0.8, "wavelength": 193, "defocus": 0.2},
    },
    "cross_quadrupole": {
        "source": {"gridSize": 101, "type": "CrossQuadrupole", "crossQuadrupole": {"radius": 0.15, "offset": 0.65}},
        "optics": {"NA": 0.8, "wavelength": 193, "defocus": 0.2},
    },
    "annular_na_0_6": {
        "source": {"gridSize": 101, "type": "Annular", "annular": {"innerRadius": 0.6, "outerRadius": 0.9}},
        "optics": {"NA": 0.6, "wavelength": 193, "defocus": 0.2},
    },
}


def execute(command: list[str], log_path: Path) -> None:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("w", encoding="utf-8") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise RuntimeError(f"command failed; see {log_path.relative_to(ROOT)}")


def make_config(name: str, case: dict) -> Path:
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    config = {
        "simulation": {"description": f"Paper evidence: {name}", "version": "1.0"},
        "mask": {
            "period": {"Lx": 1024, "Ly": 1024},
            "size": {"maskSizeX": 1024, "maskSizeY": 1024},
            "type": "Import",
            "inputFile": "../input/mask/1024x1024.bin",
            "dose": 1.0,
        },
        "source": case["source"],
        "optics": case["optics"],
        "kernel": {"count": 10, "targetIntensity": 0.225},
        "output": {"baseDir": "out"},
    }
    path = CONFIG_DIR / f"{name}.json"
    path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    return path


def read_rows(path: Path, key: str) -> dict[str, dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        return {row[key]: row for row in csv.DictReader(stream)}


def read_meta(path: Path) -> dict[str, int]:
    return {key: int(value) for key, value in (line.split() for line in path.read_text().splitlines())}


def collect() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, case in CASES.items():
        config_path = make_config(name, case)
        golden_dir = DATA_DIR / "golden" / name
        run_dir = RUN_DIR / name
        execute(
            [sys.executable, "validation/golden/run_verification.py", "--config", str(config_path.relative_to(ROOT)), "--output", str(golden_dir.relative_to(ROOT)), "--quiet"],
            run_dir / "golden.log",
        )
        execute(
            [sys.executable, "source/host/full_platform/scripts/python/run_full_platform_validation.py", "--config", str(config_path.relative_to(ROOT)), "--golden-output", str(golden_dir.relative_to(ROOT)), "--output-dir", str(run_dir.relative_to(ROOT)), "--no-visualize", "--skip-ddr-readback"],
            run_dir / "board.log",
        )
        metrics = read_rows(run_dir / "metrics.csv", "name")
        timing = read_rows(run_dir / "timing.csv", "step")
        meta = read_meta(golden_dir / "fft_meta.txt")
        implementation = metrics["host_FI_vs_golden_SOCS"]
        model = metrics["host_FI_vs_TCC_direct"]
        rows.append(
            {
                "case": name,
                "source_type": case["source"]["type"],
                "na": case["optics"]["NA"],
                "wavelength_nm": case["optics"]["wavelength"],
                "defocus_nm": case["optics"]["defocus"],
                "nx": meta["Nx"],
                "ny": meta["Ny"],
                "implementation_rmse": implementation["rmse"],
                "implementation_max_abs": implementation["max_abs"],
                "implementation_psnr_db": implementation["psnr"],
                "model_rmse_vs_tcc": model["rmse"],
                "model_max_abs_vs_tcc": model["max_abs"],
                "fpga_host_observed_seconds": timing["fpga_compute"]["elapsed_seconds"],
                "host_fi_seconds": timing["host_fi_inverse_aerial"]["elapsed_seconds"],
                "passed": implementation["passed"],
                "raw_run": run_dir.relative_to(ROOT).as_posix(),
            }
        )
        print(f"{name}: Nx={meta['Nx']}, RMSE={float(implementation['rmse']):.3e}")

    output = DATA_DIR / "optical_configuration_accuracy.csv"
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (DATA_DIR / "optical_configuration_summary.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "generated_at_utc": datetime.now(timezone.utc).isoformat(),
                "tested_cases": len(rows),
                "all_passed": all(row["passed"] == "True" for row in rows),
                "limitations": [
                    "Quasar is not implemented by the current CPU source generator and was not tested.",
                    "Only configurations yielding Nx and Ny at or below the V18 limit of 8 were sent to the board.",
                    "Defocus trials at 10 nm and 100 nm were retained as failed model-coverage cases because 10-kernel SOCS differed materially from full TCC.",
                    "FPGA timing is host-observed and not a hardware cycle count.",
                ],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (DATA_DIR / "OPTICAL_CONFIG_COVERAGE_BLOCKER.json").write_text(
        json.dumps(
            {
                "missing_source_type": "Quasar",
                "reason": "The current source generator implements Annular, Dipole, CrossQuadrupole, Point, and Import, but no Quasar model.",
                "required_action": "Define and validate a Quasar source parameterization before claiming Quasar support.",
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (DATA_DIR / "defocus_model_coverage.json").write_text(
        json.dumps(
            {
                "evidence_type": "software model truncation negative result",
                "configuration": "Annular 0.6/0.9, NA=0.8, wavelength=193 nm, nk=10, 1024x1024",
                "cases": [
                    {
                        "defocus_nm": 10.0,
                        "max_relative_difference_socs_vs_tcc": 0.29543,
                        "status": "failed software model tolerance; not sent to FPGA",
                        "raw_log": "experiments/runs/E4_optical_config/annular_defocus_10nm/golden.log",
                    },
                    {
                        "defocus_nm": 100.0,
                        "max_relative_difference_socs_vs_tcc": 0.43531,
                        "status": "failed software model tolerance; not sent to FPGA",
                        "raw_log": "experiments/runs/E4_optical_config/annular_defocus_100nm/golden.log",
                    },
                ],
                "interpretation": "The 10-kernel SOCS model is insufficient for these defocus settings; this is not an FPGA implementation error.",
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {output.relative_to(ROOT)}")


if __name__ == "__main__":
    collect()