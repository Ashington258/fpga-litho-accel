#!/usr/bin/env python3
"""
SOCS Host - Python Runner for FPGA-Litho Preprocessing Pipeline
===============================================================

Usage:
    python run.py --config <json_file> [--mode <mode>] [--output <dir>] [--verify]

Modes:
    --full           Full pipeline (mask → TCC → kernels → FPGA input)
    --compute        Compute kernels from TCC (no mask processing)
    --load           Load precomputed kernels from directory
    --verify         Compare output with golden reference (litho.cpp)

Examples:
    python run.py --config golden_original.json --mode full
    python run.py --config config_Nx16.json --mode full --verify
"""

import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path

# Configuration
SCRIPT_DIR = Path(__file__).parent.absolute()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_CONFIG = PROJECT_ROOT / "input" / "config" / "golden_original.json"
DEFAULT_OUTPUT = SCRIPT_DIR / "output"
EXE_NAME = "socs_host.exe" if sys.platform == "win32" else "socs_host"
DLL_NAMES = ["fftw3.dll", "fftw3f.dll"] if sys.platform == "win32" else []


def find_executable():
    """Find socs_host executable"""
    exe_path = SCRIPT_DIR / EXE_NAME
    if exe_path.exists():
        return exe_path

    # Try building if not found
    print(f"[run.py] Executable not found, attempting to build...")
    build_result = build_executable()
    if build_result:
        return SCRIPT_DIR / EXE_NAME
    return None


def build_executable():
    """Build socs_host executable"""
    if sys.platform == "win32":
        # Use g++ directly on Windows (MinGW)
        vcpkg_root = "C:/vcpkg/installed/x64-windows"
        include_dirs = [f"-I{vcpkg_root}/include/eigen3", f"-I{vcpkg_root}/include"]
        # Use full .lib paths (MinGW can link MSVC .lib files directly)
        lib_fftw3 = f"{vcpkg_root}/lib/fftw3.lib"
        lib_fftw3f = f"{vcpkg_root}/lib/fftw3f.lib"

        # Source files
        sources = [
            "json_parser.cpp",
            "mask_processor.cpp",
            "kernel_loader.cpp",
            "file_io.cpp",
            "mask.cpp",
            "source_processor.cpp",
            "tcc_processor.cpp",
            "kernel_extractor.cpp",
            "socs_host.cpp",
        ]

        # Compile command - use full .lib paths instead of -l flags
        compile_cmd = [
            "g++",
            "-O2",
            "-std=c++17",
            *include_dirs,
            *sources,
            lib_fftw3,
            lib_fftw3f,
            "-o",
            EXE_NAME,
        ]

        print(f"[run.py] Building with: {' '.join(compile_cmd)}")
        result = subprocess.run(
            compile_cmd, cwd=SCRIPT_DIR, capture_output=True, text=True
        )

        if result.returncode != 0:
            print(f"[run.py] Build failed: {result.stderr}")
            return False

        # Copy DLLs
        for dll in DLL_NAMES:
            dll_src = Path(vcpkg_root) / "bin" / dll
            dll_dst = SCRIPT_DIR / dll
            if dll_src.exists() and not dll_dst.exists():
                shutil.copy(dll_src, dll_dst)
                print(f"[run.py] Copied {dll}")

        print(f"[run.py] Build successful: {EXE_NAME}")
        return True
    else:
        # Use Makefile on Linux
        result = subprocess.run(
            ["make"], cwd=SCRIPT_DIR, capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"[run.py] Build failed: {result.stderr}")
            return False
        return True


def run_pipeline(config_path, mode="full", output_dir=None, verify=False):
    """Run the preprocessing pipeline"""
    exe_path = find_executable()
    if not exe_path:
        print("[run.py] ERROR: Cannot find or build executable")
        return False

    # Resolve paths
    config_path = Path(config_path).absolute()
    if not config_path.exists():
        print(f"[run.py] ERROR: Config file not found: {config_path}")
        return False

    output_dir = Path(output_dir) if output_dir else DEFAULT_OUTPUT
    output_dir = output_dir.absolute()
    output_dir.mkdir(parents=True, exist_ok=True)

    # Build command
    if mode == "full":
        cmd = [str(exe_path), "--full", str(config_path), str(output_dir)]
    elif mode == "compute":
        cmd = [str(exe_path), "--compute-kernels", str(config_path), str(output_dir)]
    elif mode == "load":
        # Need kernel_dir argument
        kernel_dir = output_dir / "kernels"
        cmd = [
            str(exe_path),
            "--load-kernels",
            str(config_path),
            str(kernel_dir),
            str(output_dir),
        ]
    else:
        print(f"[run.py] ERROR: Unknown mode: {mode}")
        return False

    print(f"[run.py] Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=SCRIPT_DIR, capture_output=True, text=True)

    print(result.stdout)
    if result.stderr:
        print(f"[run.py] stderr: {result.stderr}")

    if result.returncode != 0:
        print(f"[run.py] Pipeline failed with code {result.returncode}")
        return False

    # Verification
    if verify:
        verify_output(output_dir, config_path)

    return True


def verify_output(output_dir, config_path):
    """Compare output with golden reference (litho.cpp)"""
    print("\n[run.py] === Verification Mode ===")

    # Check for validation/golden
    golden_dir = PROJECT_ROOT / "validation" / "golden"
    litho_exe = golden_dir / "litho_full"

    if sys.platform == "win32":
        litho_exe = golden_dir / "litho_full.exe"

    if not litho_exe.exists():
        print(f"[run.py] Golden reference not found: {litho_exe}")
        print("[run.py] Please build validation/golden first:")
        print("  cd validation/golden && make")
        return

    # Run golden reference
    golden_output = golden_dir / "output"
    golden_cmd = [str(litho_exe), str(config_path), str(golden_output)]

    print(f"[run.py] Running golden: {' '.join(golden_cmd)}")
    result = subprocess.run(golden_cmd, cwd=golden_dir, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"[run.py] Golden run failed: {result.stderr}")
        return

    # Compare outputs
    print("[run.py] Comparing outputs...")
    compare_outputs(output_dir, golden_output)


def compare_outputs(socs_output, golden_output):
    """Compare SOCS output with golden reference"""
    import numpy as np

    # Compare scales
    scales_socs = socs_output / "scales.bin"
    scales_golden = golden_output / "scales.bin"

    if scales_socs.exists() and scales_golden.exists():
        s1 = np.fromfile(scales_socs, dtype=np.float32)
        s2 = np.fromfile(scales_golden, dtype=np.float32)
        diff = np.abs(s1 - s2).max()
        print(f"[run.py] scales.bin max diff: {diff:.6e}")

    # Compare kernels
    kernels_socs = socs_output / "kernels"
    kernels_golden = golden_output / "kernels"

    if kernels_socs.exists() and kernels_golden.exists():
        for i in range(4):  # Compare first 4 kernels
            krn_r_socs = kernels_socs / f"krn_{i}_r.bin"
            krn_r_golden = kernels_golden / f"krn_{i}_r.bin"

            if krn_r_socs.exists() and krn_r_golden.exists():
                k1 = np.fromfile(krn_r_socs, dtype=np.float32)
                k2 = np.fromfile(krn_r_golden, dtype=np.float32)
                diff = np.abs(k1 - k2).max()
                print(f"[run.py] krn_{i}_r.bin max diff: {diff:.6e}")


def clean_build_artifacts():
    """Clean .o, .dll, .exe files from source directory"""
    print("[run.py] Cleaning build artifacts...")

    artifacts = ["*.o", "*.obj"]
    if sys.platform == "win32":
        artifacts.extend(["*.dll", "*.exe"])

    for pattern in artifacts:
        for file in SCRIPT_DIR.glob(pattern):
            print(f"[run.py] Removing: {file}")
            file.unlink()

    print("[run.py] Clean complete")


def main():
    parser = argparse.ArgumentParser(description="SOCS Host Runner")
    parser.add_argument(
        "--config",
        type=str,
        default=str(DEFAULT_CONFIG),
        help="Configuration JSON file",
    )
    parser.add_argument(
        "--mode",
        type=str,
        default="full",
        choices=["full", "compute", "load"],
        help="Pipeline mode",
    )
    parser.add_argument(
        "--output", type=str, default=str(DEFAULT_OUTPUT), help="Output directory"
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Compare with golden reference (litho.cpp)",
    )
    parser.add_argument("--build", action="store_true", help="Build executable only")
    parser.add_argument("--clean", action="store_true", help="Clean build artifacts")

    args = parser.parse_args()

    if args.clean:
        clean_build_artifacts()
        return

    if args.build:
        build_executable()
        return

    success = run_pipeline(args.config, args.mode, args.output, args.verify)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
