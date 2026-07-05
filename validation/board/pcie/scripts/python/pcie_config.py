#!/usr/bin/env python3
"""Configuration and data layout helpers for PCIe board validation."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
from typing import Any

import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[5]
DEFAULT_CONFIG_PATH = (
    PROJECT_ROOT / "validation/board/pcie/config/pcie_validation_config.json"
)


@dataclass(frozen=True)
class DevicePaths:
    h2c: str
    c2h: str
    register: str
    register_access: str


@dataclass(frozen=True)
class AddressMap:
    control: int
    control_r: int
    mskf_r: int
    mskf_i: int
    scales: int
    krn_r: int
    krn_i: int
    tmpImg_ddr: int
    output: int


@dataclass(frozen=True)
class ValidationLimits:
    max_kernel_size: int
    max_mask_floats: int
    max_kernel_floats: int
    max_output_floats: int
    output_floats: int
    dma_chunk_bytes: int
    ddr_readback_bytes: int
    poll_interval_seconds: float
    timeout_seconds: float


@dataclass(frozen=True)
class Thresholds:
    rmse: float
    max_abs: float
    relative: float


@dataclass(frozen=True)
class PCIeValidationConfig:
    devices: DevicePaths
    addresses: AddressMap
    limits: ValidationLimits
    thresholds: Thresholds


@dataclass(frozen=True)
class GoldenMeta:
    lx: int
    ly: int
    nx: int
    ny: int
    conv_size_x: int
    conv_size_y: int
    fft_conv_size_x: int
    fft_conv_size_y: int
    kernel_count: int
    kernel_size_x: int
    kernel_size_y: int


@dataclass(frozen=True)
class GoldenData:
    output_dir: Path
    golden_output_path: Path
    meta: GoldenMeta
    mskf_r: np.ndarray
    mskf_i: np.ndarray
    scales: np.ndarray
    krn_r: np.ndarray
    krn_i: np.ndarray
    golden_output: np.ndarray


def parse_int(value: Any) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"cannot parse integer from {value!r}")


def load_pcie_config(path: Path = DEFAULT_CONFIG_PATH) -> PCIeValidationConfig:
    with path.open("r", encoding="utf-8") as f:
        raw = json.load(f)

    devices = raw["devices"]
    addresses = raw["address_map"]
    limits = raw["limits"]
    verification = raw["verification"]

    return PCIeValidationConfig(
        devices=DevicePaths(
            h2c=devices["h2c"],
            c2h=devices["c2h"],
            register=devices.get("register", devices.get("user", "")),
            register_access=devices.get("register_access", "dma"),
        ),
        addresses=AddressMap(
            control=parse_int(addresses["control"]),
            control_r=parse_int(addresses["control_r"]),
            mskf_r=parse_int(addresses["mskf_r"]),
            mskf_i=parse_int(addresses["mskf_i"]),
            scales=parse_int(addresses["scales"]),
            krn_r=parse_int(addresses["krn_r"]),
            krn_i=parse_int(addresses["krn_i"]),
            tmpImg_ddr=parse_int(addresses["tmpImg_ddr"]),
            output=parse_int(addresses["output"]),
        ),
        limits=ValidationLimits(
            max_kernel_size=parse_int(limits["max_kernel_size"]),
            max_mask_floats=parse_int(limits.get("max_mask_floats", 1048576)),
            max_kernel_floats=parse_int(limits.get("max_kernel_floats", 76832)),
            max_output_floats=parse_int(limits["max_output_floats"]),
            output_floats=parse_int(limits["output_floats"]),
            dma_chunk_bytes=parse_int(limits["dma_chunk_bytes"]),
            ddr_readback_bytes=parse_int(limits.get("ddr_readback_bytes", 4096)),
            poll_interval_seconds=float(limits["poll_interval_seconds"]),
            timeout_seconds=float(limits["timeout_seconds"]),
        ),
        thresholds=Thresholds(
            rmse=float(verification["rmse_threshold"]),
            max_abs=float(verification["max_abs_threshold"]),
            relative=float(verification["relative_threshold"]),
        ),
    )


def resolve_project_path(path: str | Path) -> Path:
    value = Path(path)
    if value.is_absolute():
        return value
    return PROJECT_ROOT / value


def load_input_config(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def expected_output_dir(config_path: Path, override: Path | None) -> Path:
    if override is not None:
        return override
    if config_path.name == "golden_1024.json":
        return PROJECT_ROOT / "output/verification"
    parts = config_path.parts
    if "Different_mask_tests" in parts:
        stem = config_path.stem
        if stem.startswith("config_"):
            stem = stem.removeprefix("config_")
        return PROJECT_ROOT / "output/Different_mask_tests" / stem
    stem = config_path.stem
    if stem.startswith("config_"):
        stem = stem.removeprefix("config_")
    if "x" in stem:
        return PROJECT_ROOT / "output/Different_resolution_tests" / stem
    return PROJECT_ROOT / "output" / stem


def parse_fft_meta(meta_path: Path) -> dict[str, int]:
    values: dict[str, int] = {}
    with meta_path.open("r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                values[parts[0]] = int(float(parts[1]))
    return values


def parse_kernel_info(kernel_info_path: Path) -> tuple[int, int, int]:
    kernel_size_x = kernel_size_y = kernel_count = 0
    with kernel_info_path.open("r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if line.startswith("- Kernel Size:"):
                value = line.split(":", 1)[1].strip().lower()
                lhs, rhs = value.split("x", 1)
                kernel_size_x = int(lhs)
                kernel_size_y = int(rhs)
            elif line.startswith("- Number of Kernels:"):
                kernel_count = int(line.split(":", 1)[1].strip())
    if not (kernel_size_x and kernel_size_y and kernel_count):
        raise ValueError(f"failed to parse kernel metadata from {kernel_info_path}")
    return kernel_size_x, kernel_size_y, kernel_count


def load_float32_file(path: Path) -> np.ndarray:
    if not path.exists():
        raise FileNotFoundError(path)
    data = np.fromfile(path, dtype=np.float32)
    if data.size == 0:
        raise ValueError(f"{path} is empty")
    return data


def load_kernel_bank(kernels_dir: Path, prefix: str, kernel_count: int) -> np.ndarray:
    chunks: list[np.ndarray] = []
    for idx in range(kernel_count):
        chunks.append(load_float32_file(kernels_dir / f"krn_{idx}_{prefix}.bin"))
    return np.concatenate(chunks).astype(np.float32, copy=False)


def find_golden_output(output_dir: Path) -> Path:
    exact = output_dir / "tmpImgp_full_128.bin"
    if exact.exists():
        return exact
    candidates = sorted(output_dir.glob("tmpImgp_full_*.bin"))
    if candidates:
        return candidates[0]
    raise FileNotFoundError(
        f"no tmpImgp_full_*.bin found in {output_dir}; "
        "run with --generate-golden to create PCIe validation data"
    )


def load_golden_data(output_dir: Path) -> GoldenData:
    meta_values = parse_fft_meta(output_dir / "fft_meta.txt")
    kernel_size_x, kernel_size_y, kernel_count = parse_kernel_info(
        output_dir / "kernels/kernel_info.txt"
    )
    golden_output_path = find_golden_output(output_dir)

    meta = GoldenMeta(
        lx=meta_values["physical_size_x"],
        ly=meta_values["physical_size_y"],
        nx=meta_values["Nx"],
        ny=meta_values["Ny"],
        conv_size_x=meta_values["conv_size_x"],
        conv_size_y=meta_values["conv_size_y"],
        fft_conv_size_x=meta_values["fft_conv_size_x"],
        fft_conv_size_y=meta_values["fft_conv_size_y"],
        kernel_count=kernel_count,
        kernel_size_x=kernel_size_x,
        kernel_size_y=kernel_size_y,
    )

    return GoldenData(
        output_dir=output_dir,
        golden_output_path=golden_output_path,
        meta=meta,
        mskf_r=load_float32_file(output_dir / "mskf_r.bin"),
        mskf_i=load_float32_file(output_dir / "mskf_i.bin"),
        scales=load_float32_file(output_dir / "scales.bin"),
        krn_r=load_kernel_bank(output_dir / "kernels", "r", kernel_count),
        krn_i=load_kernel_bank(output_dir / "kernels", "i", kernel_count),
        golden_output=load_float32_file(golden_output_path),
    )


def validate_golden_against_config(
    input_config: dict[str, Any],
    golden: GoldenData,
    pcie_config: PCIeValidationConfig,
) -> None:
    mask = input_config.get("mask", {})
    period = mask.get("period", {})
    kernel = input_config.get("kernel", {})

    expected_lx = int(period.get("Lx", golden.meta.lx))
    expected_ly = int(period.get("Ly", golden.meta.ly))
    expected_nk = int(kernel.get("count", golden.meta.kernel_count))

    problems: list[str] = []
    if golden.meta.lx != expected_lx:
        problems.append(f"Lx mismatch: config={expected_lx}, fft_meta={golden.meta.lx}")
    if golden.meta.ly != expected_ly:
        problems.append(f"Ly mismatch: config={expected_ly}, fft_meta={golden.meta.ly}")
    if golden.meta.kernel_count != expected_nk:
        problems.append(
            f"kernel count mismatch: config={expected_nk}, data={golden.meta.kernel_count}"
        )
    if golden.scales.size != golden.meta.kernel_count:
        problems.append(
            f"scales length mismatch: scales={golden.scales.size}, kernels={golden.meta.kernel_count}"
        )
    if golden.meta.kernel_size_x > pcie_config.limits.max_kernel_size:
        problems.append(
            f"kernel width {golden.meta.kernel_size_x} exceeds HLS limit {pcie_config.limits.max_kernel_size}"
        )
    if golden.meta.kernel_size_y > pcie_config.limits.max_kernel_size:
        problems.append(
            f"kernel height {golden.meta.kernel_size_y} exceeds HLS limit {pcie_config.limits.max_kernel_size}"
        )
    if golden.golden_output.size > pcie_config.limits.max_output_floats:
        problems.append(
            f"golden output length {golden.golden_output.size} exceeds HLS output limit "
            f"{pcie_config.limits.max_output_floats}"
        )
    if golden.golden_output.size != pcie_config.limits.output_floats:
        problems.append(
            f"golden output length {golden.golden_output.size} from "
            f"{golden.golden_output_path.name} != expected "
            f"{pcie_config.limits.output_floats}; run with --generate-golden "
            "to create tmpImgp_full_128.bin for the fixed 128x128 V18 IP"
        )

    expected_mask_floats = golden.meta.lx * golden.meta.ly
    if expected_mask_floats > pcie_config.limits.max_mask_floats:
        problems.append(
            f"mask length Lx*Ly={expected_mask_floats} exceeds HLS AXI depth "
            f"{pcie_config.limits.max_mask_floats}"
        )
    if golden.mskf_r.size != expected_mask_floats:
        problems.append(
            f"mskf_r length {golden.mskf_r.size} != Lx*Ly {expected_mask_floats}"
        )
    if golden.mskf_i.size != expected_mask_floats:
        problems.append(
            f"mskf_i length {golden.mskf_i.size} != Lx*Ly {expected_mask_floats}"
        )

    expected_kernel_floats = (
        golden.meta.kernel_count * golden.meta.kernel_size_x * golden.meta.kernel_size_y
    )
    if expected_kernel_floats > pcie_config.limits.max_kernel_floats:
        problems.append(
            f"kernel bank length {expected_kernel_floats} exceeds HLS AXI depth "
            f"{pcie_config.limits.max_kernel_floats}"
        )
    if golden.krn_r.size != expected_kernel_floats:
        problems.append(
            f"krn_r length {golden.krn_r.size} != expected {expected_kernel_floats}"
        )
    if golden.krn_i.size != expected_kernel_floats:
        problems.append(
            f"krn_i length {golden.krn_i.size} != expected {expected_kernel_floats}"
        )

    if problems:
        joined = "\n  - ".join(problems)
        raise ValueError(f"golden data is not compatible:\n  - {joined}")

    validate_ddr_layout(golden, pcie_config)


def validate_ddr_layout(golden: GoldenData, pcie_config: PCIeValidationConfig) -> None:
    """Ensure configured DDR regions do not overlap for the loaded dataset."""

    addresses = pcie_config.addresses
    regions = [
        ("mskf_r", addresses.mskf_r, golden.mskf_r.nbytes),
        ("mskf_i", addresses.mskf_i, golden.mskf_i.nbytes),
        ("scales", addresses.scales, golden.scales.nbytes),
        ("krn_r", addresses.krn_r, golden.krn_r.nbytes),
        ("krn_i", addresses.krn_i, golden.krn_i.nbytes),
        ("tmpImg_ddr", addresses.tmpImg_ddr, pcie_config.limits.output_floats * 4),
        ("output", addresses.output, pcie_config.limits.output_floats * 4),
    ]
    ordered = sorted(regions, key=lambda item: item[1])
    problems: list[str] = []

    for (name, start, size), (next_name, next_start, _next_size) in zip(
        ordered, ordered[1:]
    ):
        end = start + size
        if end > next_start:
            problems.append(
                f"{name} [0x{start:08x}, 0x{end:08x}) overlaps {next_name} "
                f"@ 0x{next_start:08x}"
            )

    if problems:
        joined = "\n  - ".join(problems)
        raise ValueError(f"PCIe DDR layout overlaps:\n  - {joined}")
