#!/usr/bin/env python3
"""Run SOCS V18 board validation through PCIe XDMA."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(Path(__file__).resolve().parent))

from pcie_config import (  # noqa: E402
    DEFAULT_CONFIG_PATH,
    PROJECT_ROOT,
    GoldenData,
    PCIeValidationConfig,
    expected_output_dir,
    load_golden_data,
    load_input_config,
    load_pcie_config,
    resolve_project_path,
    validate_golden_against_config,
)
from xdma import XDMADevice, XDMAError  # noqa: E402


CONTROL_REGS = {
    "ap_ctrl": 0x00,
    "nk": 0x10,
    "nx_actual": 0x18,
    "ny_actual": 0x20,
    "Lx": 0x28,
    "Ly": 0x30,
}

CONTROL_R_REGS = {
    "mskf_r": 0x10,
    "mskf_i": 0x1C,
    "krn_r": 0x28,
    "krn_i": 0x34,
    "scales": 0x40,
    "tmpImg_ddr": 0x4C,
    "output": 0x58,
}


def write_checked_reg32(dev: XDMADevice, address: int, value: int, label: str) -> None:
    """Write an AXI-Lite register and verify by immediate readback."""

    expected = value & 0xFFFFFFFF
    dev.write_reg32(address, expected)
    actual = dev.read_reg32(address)
    if actual != expected:
        raise RuntimeError(
            f"register readback mismatch for {label} @ 0x{address:08x}: "
            f"wrote 0x{expected:08x}, read 0x{actual:08x}"
        )


def verify_float_array_readback(
    dev: XDMADevice,
    name: str,
    address: int,
    array: np.ndarray,
    max_bytes: int,
) -> None:
    """Read back representative array chunks after DMA write."""

    expected = np.ascontiguousarray(array, dtype=np.float32)
    if expected.size == 0 or max_bytes <= 0:
        return

    sample_count = min(expected.size, max(1, max_bytes // np.dtype(np.float32).itemsize))
    max_start = max(0, expected.size - sample_count)
    starts = {0}

    nonzero = np.flatnonzero(expected != 0.0)
    if nonzero.size:
        starts.add(min(int(nonzero[0]), max_start))
        starts.add(min(int(nonzero[-1]), max_start))

    for start in sorted(starts):
        actual = dev.read_float_array(address + start * 4, sample_count)
        ref = expected[start : start + sample_count]
        if actual.size != ref.size:
            raise RuntimeError(
                f"{name} readback size mismatch at float offset {start}: "
                f"actual={actual.size}, expected={ref.size}"
            )
        diff = np.abs(actual.astype(np.float64) - ref.astype(np.float64))
        max_abs = float(np.max(diff)) if diff.size else 0.0
        if max_abs != 0.0:
            raise RuntimeError(
                f"{name} DDR readback mismatch at float offset {start}: "
                f"max_abs={max_abs:.10e}"
            )

    print(
        f"[DMA C2H] {name:7s}: readback PASS "
        f"({len(starts)} sample window(s), {sample_count} floats/window)",
        flush=True,
    )


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

    print("[GOLDEN] Generating data from JSON config")
    print("         " + " ".join(cmd))
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        raise RuntimeError(f"golden generation failed with exit code {result.returncode}")


def print_layout(config: PCIeValidationConfig, golden: GoldenData) -> None:
    addresses = config.addresses
    print("[DEVICE] XDMA access")
    print(f"  h2c:             {config.devices.h2c}")
    print(f"  c2h:             {config.devices.c2h}")
    print(f"  register_access: {config.devices.register_access}")
    print(f"  register:        {config.devices.register or '<via h2c/c2h DMA>'}")

    print("[LAYOUT] DDR regions")
    rows = [
        ("mskf_r", addresses.mskf_r, golden.mskf_r.nbytes),
        ("mskf_i", addresses.mskf_i, golden.mskf_i.nbytes),
        ("scales", addresses.scales, golden.scales.nbytes),
        ("krn_r", addresses.krn_r, golden.krn_r.nbytes),
        ("krn_i", addresses.krn_i, golden.krn_i.nbytes),
        ("tmpImg_ddr", addresses.tmpImg_ddr, config.limits.output_floats * 4),
        ("output", addresses.output, config.limits.output_floats * 4),
    ]
    for name, address, size in rows:
        print(f"  {name:10s} 0x{address:08x}  {size:>10,d} bytes", flush=True)

    print("[LAYOUT] HLS parameters", flush=True)
    print(
        f"  Lx={golden.meta.lx}, Ly={golden.meta.ly}, "
        f"Nx={golden.meta.nx}, Ny={golden.meta.ny}, nk={golden.meta.kernel_count}",
        flush=True,
    )
    print(
        f"  kernel={golden.meta.kernel_size_x}x{golden.meta.kernel_size_y}, "
        f"output={config.limits.output_floats} floats",
        flush=True,
    )


def configure_hls(dev: XDMADevice, config: PCIeValidationConfig, golden: GoldenData) -> None:
    base = config.addresses.control
    ptr_base = config.addresses.control_r
    addresses = config.addresses

    pointer_values = {
        "mskf_r": addresses.mskf_r,
        "mskf_i": addresses.mskf_i,
        "krn_r": addresses.krn_r,
        "krn_i": addresses.krn_i,
        "scales": addresses.scales,
        "tmpImg_ddr": addresses.tmpImg_ddr,
        "output": addresses.output,
    }
    for name, value in pointer_values.items():
        offset = CONTROL_R_REGS[name]
        try:
            write_checked_reg32(
                dev,
                ptr_base + offset,
                value & 0xFFFFFFFF,
                f"control_r.{name}.low",
            )
            write_checked_reg32(
                dev,
                ptr_base + offset + 4,
                (value >> 32) & 0xFFFFFFFF,
                f"control_r.{name}.high",
            )
        except (OSError, XDMAError) as exc:
            raise RuntimeError(
                "HLS AXI-Lite pointer register access failed. "
                f"Register control_r.{name} is mapped at base 0x{ptr_base:08x}, "
                f"offset 0x{offset:02x}; current register_access="
                f"{config.devices.register_access!r}. If DDR DMA works but this "
                "times out, the bitstream/PCIe BAR mapping does not expose "
                "HLS s_axi_control_r to XDMA. Use --dma-only for data-path "
                "validation or rebuild the Vivado BD so XDMA M_AXI/user BAR "
                "can reach 0x00010000."
            ) from exc

    scalar_values = {
        "nk": golden.meta.kernel_count,
        "nx_actual": golden.meta.nx,
        "ny_actual": golden.meta.ny,
        "Lx": golden.meta.lx,
        "Ly": golden.meta.ly,
    }
    for name, value in scalar_values.items():
        try:
            write_checked_reg32(dev, base + CONTROL_REGS[name], value, f"control.{name}")
        except (OSError, XDMAError) as exc:
            raise RuntimeError(
                "HLS AXI-Lite scalar register access failed. "
                f"Register control.{name} is mapped at base 0x{base:08x}, "
                f"offset 0x{CONTROL_REGS[name]:02x}; current register_access="
                f"{config.devices.register_access!r}. If DDR DMA works but this "
                "times out, the bitstream/PCIe BAR mapping does not expose "
                "HLS s_axi_control to XDMA. Use --dma-only for data-path "
                "validation or rebuild the Vivado BD so XDMA M_AXI/user BAR "
                "can reach 0x00000000."
            ) from exc

    print("[HLS] AXI-Lite register configuration readback PASS", flush=True)


def write_inputs(
    dev: XDMADevice,
    config: PCIeValidationConfig,
    golden: GoldenData,
    verify_readback: bool,
) -> None:
    addresses = config.addresses
    transfers = [
        ("mskf_r", addresses.mskf_r, golden.mskf_r),
        ("mskf_i", addresses.mskf_i, golden.mskf_i),
        ("scales", addresses.scales, golden.scales),
        ("krn_r", addresses.krn_r, golden.krn_r),
        ("krn_i", addresses.krn_i, golden.krn_i),
    ]
    for name, address, array in transfers:
        print(f"[DMA H2C] {name:7s}: writing {array.nbytes:,d} bytes", flush=True)
        start = time.monotonic()
        dev.write_float_array(address, array)
        elapsed = time.monotonic() - start
        mib = array.nbytes / (1024 * 1024)
        rate = mib / elapsed if elapsed > 0 else 0.0
        print(f"[DMA H2C] {name:7s}: {mib:7.2f} MiB in {elapsed:.3f}s ({rate:.2f} MiB/s)", flush=True)
        if verify_readback:
            verify_float_array_readback(
                dev,
                name,
                address,
                array,
                config.limits.ddr_readback_bytes,
            )

    zero = np.zeros(config.limits.output_floats, dtype=np.float32)
    print("[DMA H2C] tmp/output: clearing output buffers", flush=True)
    dev.write_float_array(addresses.tmpImg_ddr, zero)
    dev.write_float_array(addresses.output, zero)
    if verify_readback:
        verify_float_array_readback(
            dev,
            "tmpImg",
            addresses.tmpImg_ddr,
            zero,
            config.limits.ddr_readback_bytes,
        )
        verify_float_array_readback(
            dev,
            "output",
            addresses.output,
            zero,
            config.limits.ddr_readback_bytes,
        )


def start_and_wait(dev: XDMADevice, config: PCIeValidationConfig) -> int:
    control_base = config.addresses.control
    before = dev.read_reg32(control_base + CONTROL_REGS["ap_ctrl"])
    print(f"[HLS] ap_ctrl before start: 0x{before:08x}")
    dev.write_reg32(control_base + CONTROL_REGS["ap_ctrl"], 0x1)
    status = dev.wait_done(
        control_base + CONTROL_REGS["ap_ctrl"],
        config.limits.timeout_seconds,
        config.limits.poll_interval_seconds,
    )
    print(f"[HLS] ap_ctrl done:         0x{status:08x}")
    return status


def read_output(dev: XDMADevice, config: PCIeValidationConfig) -> np.ndarray:
    start = time.monotonic()
    output = dev.read_float_array(config.addresses.output, config.limits.output_floats)
    elapsed = time.monotonic() - start
    mib = output.nbytes / (1024 * 1024)
    rate = mib / elapsed if elapsed > 0 else 0.0
    print(f"[DMA C2H] output : {mib:7.2f} MiB in {elapsed:.3f}s ({rate:.2f} MiB/s)")
    return output


def compare_output(
    hls_output: np.ndarray,
    golden: GoldenData,
    config: PCIeValidationConfig,
) -> bool:
    golden_output = golden.golden_output
    if hls_output.size != golden_output.size:
        raise ValueError(
            f"output size mismatch: hls={hls_output.size}, golden={golden_output.size}"
        )

    diff = hls_output.astype(np.float64) - golden_output.astype(np.float64)
    abs_diff = np.abs(diff)
    rmse = float(np.sqrt(np.mean(diff * diff)))
    max_abs = float(np.max(abs_diff))
    denom = np.maximum(np.abs(golden_output.astype(np.float64)), 1e-12)
    max_rel = float(np.max(abs_diff / denom))

    print(f"[COMPARE] HLS output vs {golden.golden_output_path.name}")
    print(f"  RMSE:       {rmse:.10e}")
    print(f"  Max abs:    {max_abs:.10e}")
    print(f"  Max rel:    {max_rel:.10e}")
    print(f"  HLS range:  [{hls_output.min():.6e}, {hls_output.max():.6e}]")
    print(f"  Ref range:  [{golden_output.min():.6e}, {golden_output.max():.6e}]")

    ok = (
        rmse <= config.thresholds.rmse
        and max_abs <= config.thresholds.max_abs
        and max_rel <= config.thresholds.relative
    )
    print("[COMPARE] " + ("PASS" if ok else "FAIL"))
    return ok


def save_output(path: Path, output: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    output.astype(np.float32, copy=False).tofile(path)
    print(f"[OUTPUT] Saved HLS output: {path}")


def run_validation(args: argparse.Namespace) -> int:
    config_path = resolve_project_path(args.config)
    pcie_config_path = resolve_project_path(args.pcie_config)
    golden_output_dir = expected_output_dir(
        config_path, resolve_project_path(args.golden_output) if args.golden_output else None
    )

    if args.generate_golden:
        generate_golden(config_path, golden_output_dir, args.no_clean)

    pcie_config = load_pcie_config(pcie_config_path)
    input_config = load_input_config(config_path)
    golden = load_golden_data(golden_output_dir)
    validate_golden_against_config(input_config, golden, pcie_config)
    print_layout(pcie_config, golden)

    if args.dry_run:
        print("[DRY-RUN] Data and register layout checks passed; XDMA was not accessed.")
        return 0

    device_paths = [
        pcie_config.devices.h2c,
        pcie_config.devices.c2h,
    ]
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
        write_inputs(dev, pcie_config, golden, not args.skip_ddr_readback)
        if args.dma_only:
            print(
                "[DMA-ONLY] Input DMA and DDR readback completed; "
                "HLS AXI-Lite start was skipped."
            )
            return 0
        configure_hls(dev, pcie_config, golden)
        start_and_wait(dev, pcie_config)
        hls_output = read_output(dev, pcie_config)

    output_path = resolve_project_path(args.output)
    save_output(output_path, hls_output)
    return 0 if compare_output(hls_output, golden, pcie_config) else 2


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="SOCS V18 PCIe XDMA board validation"
    )
    parser.add_argument(
        "--config",
        default="input/config/golden_1024.json",
        help="Input JSON config used by validation/golden",
    )
    parser.add_argument(
        "--pcie-config",
        default=str(DEFAULT_CONFIG_PATH.relative_to(PROJECT_ROOT)),
        help="PCIe device/address configuration JSON",
    )
    parser.add_argument(
        "--golden-output",
        default=None,
        help="Directory containing generated golden bin files",
    )
    parser.add_argument(
        "--output",
        default="validation/board/pcie/output/aerial_image_output.bin",
        help="Path to save FPGA output bin",
    )
    parser.add_argument(
        "--generate-golden",
        action="store_true",
        help="Generate golden data from --config before running PCIe validation",
    )
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Pass --no-clean to golden generation",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate files and layout without accessing XDMA devices",
    )
    parser.add_argument(
        "--dma-only",
        action="store_true",
        help="Write/readback DDR via XDMA but skip HLS AXI-Lite configuration and execution",
    )
    parser.add_argument(
        "--skip-ddr-readback",
        action="store_true",
        help="Skip representative DDR readback checks after each DMA write",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return run_validation(args)
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
