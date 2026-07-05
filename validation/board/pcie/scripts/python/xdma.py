#!/usr/bin/env python3
"""Small XDMA userspace access wrapper."""

from __future__ import annotations

from pathlib import Path
import os
import struct
import time

import numpy as np


class XDMAError(RuntimeError):
    """Raised when an XDMA device access fails."""


class XDMADevice:
    """Access Xilinx XDMA character devices with positional reads/writes."""

    REGISTER_ACCESS_MODES = {"dma", "user", "control"}

    def __init__(
        self,
        h2c_path: str = "/dev/xdma0_h2c_0",
        c2h_path: str = "/dev/xdma0_c2h_0",
        register_path: str = "",
        register_access: str = "dma",
        chunk_bytes: int = 4 * 1024 * 1024,
    ) -> None:
        self.h2c_path = Path(h2c_path)
        self.c2h_path = Path(c2h_path)
        if register_access not in self.REGISTER_ACCESS_MODES:
            raise ValueError(
                f"invalid register_access={register_access!r}; "
                f"expected one of {sorted(self.REGISTER_ACCESS_MODES)}"
            )
        self.register_path = Path(register_path) if register_path else None
        self.register_access = register_access
        self.chunk_bytes = chunk_bytes
        self._h2c_fd: int | None = None
        self._c2h_fd: int | None = None
        self._register_fd: int | None = None

    def __enter__(self) -> "XDMADevice":
        self.open()
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def open(self) -> None:
        self._h2c_fd = os.open(self.h2c_path, os.O_WRONLY)
        self._c2h_fd = os.open(self.c2h_path, os.O_RDONLY)
        if self.register_access in {"user", "control"}:
            if self.register_path is None:
                raise FileNotFoundError(
                    f"register_access={self.register_access} requires a register device path"
                )
            self._register_fd = os.open(self.register_path, os.O_RDWR)
        else:
            self._register_fd = None

    def close(self) -> None:
        for fd in (self._h2c_fd, self._c2h_fd, self._register_fd):
            if fd is not None:
                os.close(fd)
        self._h2c_fd = None
        self._c2h_fd = None
        self._register_fd = None

    @staticmethod
    def ensure_devices_exist(paths: list[str | None]) -> None:
        missing = [path for path in paths if path and not Path(path).exists()]
        if missing:
            joined = ", ".join(missing)
            raise FileNotFoundError(f"XDMA device file(s) not found: {joined}")

    def write_dma(self, address: int, data: bytes | bytearray | memoryview) -> None:
        if self._h2c_fd is None:
            raise XDMAError("XDMA h2c device is not open")
        view = memoryview(data)
        total = len(view)
        written = 0
        while written < total:
            end = min(written + self.chunk_bytes, total)
            chunk = view[written:end]
            try:
                count = os.pwrite(self._h2c_fd, chunk, address + written)
            except OSError as exc:
                raise XDMAError(
                    f"DMA H2C write failed at 0x{address + written:08x} "
                    f"({len(chunk)} bytes): {exc}"
                ) from exc
            if count <= 0:
                raise XDMAError(f"DMA write failed at 0x{address + written:08x}")
            written += count

    def read_dma(self, address: int, size: int) -> bytes:
        if self._c2h_fd is None:
            raise XDMAError("XDMA c2h device is not open")
        chunks: list[bytes] = []
        read_total = 0
        while read_total < size:
            count = min(self.chunk_bytes, size - read_total)
            try:
                data = os.pread(self._c2h_fd, count, address + read_total)
            except OSError as exc:
                raise XDMAError(
                    f"DMA C2H read failed at 0x{address + read_total:08x} "
                    f"({count} bytes): {exc}"
                ) from exc
            if not data:
                raise XDMAError(f"DMA read failed at 0x{address + read_total:08x}")
            chunks.append(data)
            read_total += len(data)
        return b"".join(chunks)

    def write_reg32(self, address: int, value: int) -> None:
        data = struct.pack("<I", value & 0xFFFFFFFF)
        if self.register_access == "dma":
            self.write_dma(address, data)
            return
        if self._register_fd is None:
            raise XDMAError(f"{self.register_access} register device is not open")
        count = os.pwrite(self._register_fd, data, address)
        if count != 4:
            raise XDMAError(f"register write failed at 0x{address:08x}")

    def read_reg32(self, address: int) -> int:
        if self.register_access == "dma":
            data = self.read_dma(address, 4)
        else:
            if self._register_fd is None:
                raise XDMAError(f"{self.register_access} register device is not open")
            data = os.pread(self._register_fd, 4, address)
        if len(data) != 4:
            raise XDMAError(f"register read failed at 0x{address:08x}")
        return struct.unpack("<I", data)[0]

    def write_u64_split(self, base: int, offset: int, value: int) -> None:
        self.write_reg32(base + offset, value & 0xFFFFFFFF)
        self.write_reg32(base + offset + 4, (value >> 32) & 0xFFFFFFFF)

    def write_float_array(self, address: int, array: np.ndarray) -> None:
        contiguous = np.ascontiguousarray(array, dtype=np.float32)
        self.write_dma(address, contiguous.tobytes(order="C"))

    def verify_float_array_prefix(
        self,
        address: int,
        array: np.ndarray,
        max_bytes: int,
    ) -> float:
        """Read back a prefix of a float32 array and return max absolute error."""

        if max_bytes <= 0:
            return 0.0
        expected = np.ascontiguousarray(array, dtype=np.float32)
        count = min(expected.size, max_bytes // np.dtype(np.float32).itemsize)
        if count == 0:
            return 0.0
        actual = self.read_float_array(address, count)
        diff = np.abs(actual.astype(np.float64) - expected[:count].astype(np.float64))
        return float(np.max(diff)) if diff.size else 0.0

    def read_float_array(self, address: int, count: int) -> np.ndarray:
        raw = self.read_dma(address, count * np.dtype(np.float32).itemsize)
        return np.frombuffer(raw, dtype=np.float32).copy()

    def wait_done(
        self,
        control_base: int,
        timeout_seconds: float,
        poll_interval_seconds: float,
    ) -> int:
        deadline = time.monotonic() + timeout_seconds
        last_status = 0
        while time.monotonic() < deadline:
            last_status = self.read_reg32(control_base)
            if last_status & 0x2:
                return last_status
            time.sleep(poll_interval_seconds)
        raise TimeoutError(
            f"HLS IP timeout after {timeout_seconds:.3f}s; last ap_ctrl=0x{last_status:08x}"
        )
