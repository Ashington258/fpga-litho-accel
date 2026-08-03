#!/usr/bin/env python3
"""Rebuild the paper experiment index from archived evidence files."""

from __future__ import annotations

import csv
import hashlib
import json
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_ROOT = ROOT / "experiments" / "data"


@dataclass(frozen=True)
class DatasetSpec:
    task: str
    directory: str
    description: str
    required_files: tuple[str, ...]


DATASETS = (
    DatasetSpec("D1", "D1_cpu_kernel", "Fair CPU SOCS kernel-only latency and numerical validation", ("cpu_kernel_latency_raw.csv", "cpu_kernel_latency_summary.json", "cpu_benchmark_environment.json")),
    DatasetSpec("D2", "D2_fpga_kernel", "On-board FPGA kernel cycles and host-observed latency", ("fpga_kernel_cycles_raw.csv", "fpga_kernel_host_wall_raw.csv", "fpga_kernel_latency_summary.json")),
    DatasetSpec("D3", "D3_end_to_end", "CPU and Host-FPGA end-to-end latency and batch throughput", ("fpga_e2e_latency_raw.csv", "fpga_e2e_stage_breakdown.csv", "batch_throughput.csv", "cpu_e2e_latency_raw.csv", "end_to_end_latency_summary.json")),
    DatasetSpec("D4", "D4_power_energy", "Synchronized CPU/FPGA power and energy measurements", ("cpu_power_trace.csv", "fpga_power_trace.csv", "power_measurement_environment.json", "energy_summary.json")),
    DatasetSpec("E1", "E1_multi_mask", "Multi-mask accuracy statistics", ("multi_mask_accuracy.csv", "multi_mask_summary.json")),
    DatasetSpec("E2", "E2_kernel_count", "SOCS kernel-count scaling and Pareto data", ("kernel_count_scaling.csv", "accuracy_latency_pareto.csv")),
    DatasetSpec("E3", "E3_kernel_size", "Runtime effective-kernel-size validation", ("kernel_size_runtime_config.csv",)),
    DatasetSpec("E4", "E4_optical_config", "Optical-configuration robustness", ("optical_configuration_accuracy.csv",)),
    DatasetSpec("E5", "E5_resolution", "Input-resolution scaling", ("resolution_scaling.csv", "resolution_stage_breakdown.csv")),
    DatasetSpec("E6", "E6_ablation", "Architecture ablation results", ("ablation_results.csv",)),
    DatasetSpec("E7", "E7_implementation", "Final implementation utilization, timing, power, and bitstream manifest", ("implementation_manifest.json",)),
    DatasetSpec("F1", "F1_paper_evidence", "Paper-number-to-source mapping", ("paper_evidence_manifest.csv",)),
    DatasetSpec("F2", "F2_golden_manifest", "Golden data metadata and hashes", ("golden_manifest.csv",)),
    DatasetSpec("F3", "F3_archive", "Reproducible experiment archive metadata", ("archive_manifest.json",)),
)


def git_value(*args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=ROOT, check=False, capture_output=True, text=True
    )
    return result.stdout.strip()


def git_ignored(path: Path) -> bool:
    result = subprocess.run(
        ["git", "check-ignore", "--quiet", str(path.relative_to(ROOT))],
        cwd=ROOT,
        check=False,
    )
    return result.returncode == 0


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def rebuild() -> None:
    generated_at = datetime.now(timezone.utc).isoformat()
    datasets = []
    file_rows = []

    for spec in DATASETS:
        directory = DATA_ROOT / spec.directory
        files = sorted(path for path in directory.rglob("*") if path.is_file()) if directory.exists() else []
        names = {path.name for path in files}
        missing = [name for name in spec.required_files if name not in names]
        blockers = [path.name for path in files if path.name.endswith("_BLOCKER.json")]

        if not files:
            status = "not_started"
        elif missing or blockers:
            status = "partial"
        else:
            status = "complete"

        datasets.append(
            {
                "task": spec.task,
                "directory": f"experiments/data/{spec.directory}",
                "description": spec.description,
                "status": status,
                "file_count": len(files),
                "missing_required_files": missing,
                "blocker_files": blockers,
            }
        )

        archived_file_count = 0
        for path in files:
            relative = path.relative_to(ROOT).as_posix()
            git_archived = not git_ignored(path)
            archived_file_count += int(git_archived)
            file_rows.append(
                (spec.task, relative, path.stat().st_size, sha256(path), git_archived)
            )
        datasets[-1]["git_archived_file_count"] = archived_file_count
        datasets[-1]["local_generated_file_count"] = len(files) - archived_file_count

    index = {
        "schema_version": 1,
        "generated_at_utc": generated_at,
        "git_commit": git_value("rev-parse", "HEAD"),
        "git_dirty": bool(git_value("status", "--porcelain")),
        "datasets": datasets,
    }
    (DATA_ROOT / "experiment_index.json").write_text(
        json.dumps(index, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    with (DATA_ROOT / "experiment_files.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("task", "file", "size_bytes", "sha256", "git_archived"))
        writer.writerows(file_rows)

    print(f"Indexed {len(file_rows)} files across {len(DATASETS)} tasks")


if __name__ == "__main__":
    rebuild()