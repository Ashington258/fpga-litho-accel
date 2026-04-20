#!/usr/bin/env python3
"""
FPGA-Litho 统一验证入口
======================
独立编译验证程序，不依赖 reference 目录

Usage:
    python run_verification.py [--config CONFIG] [--debug]

Examples:
    python run_verification.py                  # 默认验证
    python run_verification.py --debug          # 调试模式
    python run_verification.py --compile-only   # 仅编译
"""

import os
import sys
import argparse
import subprocess
import shutil
from pathlib import Path
import numpy as np

# 项目根目录（动态推导）
PROJECT_ROOT = (
    Path(__file__).resolve().parent.parent.parent
)  # validation/golden -> validation -> project_root

# ============================================================================
# 编译函数
# ============================================================================


def compile_litho() -> bool:
    """编译 Litho 验证程序"""
    cpp_dir = PROJECT_ROOT / "validation/golden/src"
    os.chdir(cpp_dir)

    print("[COMPILE] Building Litho...")

    # 检测操作系统，选择合适的编译命令
    if sys.platform == "win32":
        # Windows: 使用 mingw32-make 和 Makefile.win
        # 设置 MSYS2 UCRT64 环境
        msys2_bin = "C:/msys64/ucrt64/bin"
        env = os.environ.copy()
        env["PATH"] = msys2_bin + ";" + env.get("PATH", "")

        # 先清理
        subprocess.run(
            ["mingw32-make", "-f", "Makefile.win", "clean"],
            capture_output=True,
            text=True,
            env=env,
        )
        # 编译
        result = subprocess.run(
            ["mingw32-make", "-f", "Makefile.win"],
            capture_output=True,
            text=True,
            env=env,
        )
    else:
        # Linux/WSL: 使用标准 make
        subprocess.run(["make", "clean"], capture_output=True, text=True)
        result = subprocess.run(["make"], capture_output=True, text=True)

    if result.returncode != 0:
        print(f"[ERROR] Compilation failed:\n{result.stderr}")
        return False

    print("[COMPILE] Litho build successful!")
    return True


# ============================================================================
# 运行函数
# ============================================================================


def run_litho(config_path: Path, output_dir: Path, verbose: int) -> bool:
    """运行 Litho 验证程序"""
    # Windows 使用 .exe 后缀
    if sys.platform == "win32":
        executable = PROJECT_ROOT / "validation/golden/src/litho.exe"
    else:
        executable = PROJECT_ROOT / "validation/golden/src/litho"

    args = [str(executable), str(config_path), str(output_dir)]
    if verbose == 2:
        args.append("--debug")
    elif verbose == 0:
        args.append("--quiet")

    print(f"\n[RUN] Starting Litho Verification...")
    print(f"  Config: {config_path}")
    print(f"  Output: {output_dir}")

    # Windows 上 litho.exe 可能输出 stderr 消息导致 returncode != 0
    # 但实际执行成功（输出文件正确生成）
    # 解决方案：忽略 stderr，通过检查关键输出文件来判断成功
    try:
        # 添加 MSYS2 UCRT64 bin 到 PATH（编译依赖）
        env = os.environ.copy()
        msys2_bin = "C:\\msys64\\ucrt64\\bin"
        if sys.platform == "win32" and os.path.exists(msys2_bin):
            env["PATH"] = msys2_bin + ";" + env.get("PATH", "")

        result = subprocess.run(
            args,
            stderr=subprocess.PIPE,  # 捕获 stderr，不显示也不影响判断
            timeout=300,  # 5 分钟超时
            env=env,
        )

        # 检查关键输出文件是否存在来判断成功
        key_files = [
            output_dir / "aerial_image_tcc_direct.bin",
            output_dir / "aerial_image_socs_kernel.bin",
        ]

        # 动态检测 HLS golden 文件名 (tmpImgp_pad*.bin)
        golden_files = list(output_dir.glob("tmpImgp_pad*.bin"))
        if golden_files:
            key_files.append(golden_files[0])

        success = all(f.exists() and f.stat().st_size > 0 for f in key_files)

        if not success and result.returncode != 0:
            print(f"  [WARN] litho.exe returned code {result.returncode}")
            if result.stderr:
                print(
                    f"  stderr: {result.stderr.decode('utf-8', errors='replace')[:200]}"
                )

        return success

    except subprocess.TimeoutExpired:
        print("[ERROR] Litho execution timed out (300s)")
        return False
    except Exception as e:
        print(f"[ERROR] Failed to run litho: {e}")
        return False


# ============================================================================
# 结果分析
# ============================================================================


def analyze_results(output_dir: Path) -> bool:
    """分析验证结果"""
    print("\n[ANALYSIS] Verification Results")
    print("=" * 50)

    # 检查文件
    expected = [
        "source.png",
        "mask.png",
        "tcc_r.bin",
        "tcc_i.bin",
        "mskf_r.bin",
        "mskf_i.bin",  # SOCS HLS 输入
        "scales.bin",  # SOCS HLS 输入
        "aerial_image_tcc_direct.bin",
        "aerial_image_tcc_direct.png",  # TCC 直接计算成像
        "aerial_image_socs_kernel.bin",
        "aerial_image_socs_kernel.png",  # SOCS kernel 重建成像
        "kernels/kernel_info.txt",
    ]

    all_found = True
    for f in expected:
        path = output_dir / f
        if path.exists():
            print(f"  ✓ {f} ({path.stat().st_size} bytes)")
        else:
            print(f"  ✗ {f} MISSING")
            all_found = False

    if not all_found:
        return False

    # 比较 TCC vs SOCS
    img_tcc = output_dir / "aerial_image_tcc_direct.bin"
    img_socs = output_dir / "aerial_image_socs_kernel.bin"

    if img_tcc.exists() and img_socs.exists():
        tcc_data = np.fromfile(str(img_tcc), dtype=np.float32)
        socs_data = np.fromfile(str(img_socs), dtype=np.float32)

        print(f"\n[COMPARISON] TCC vs SOCS:")
        print(f"  TCC  Mean: {np.mean(tcc_data):.6f}")
        print(f"  SOCS Mean: {np.mean(socs_data):.6f}")

        max_diff = np.max(np.abs(socs_data - tcc_data))
        rel_diff = max_diff / max(np.max(tcc_data), 1e-6)
        print(f"  Max Relative Diff: {rel_diff:.4e}")

        # SOCS 使用有限核数，~5% 截断误差是正常的
        if rel_diff < 0.1:
            print("  ✓ VERIFICATION PASSED (within nk truncation tolerance)")
            return True
        else:
            print("  ✗ VERIFICATION FAILED")
            return False

    return False


# ============================================================================
# 主函数
# ============================================================================


def main():
    parser = argparse.ArgumentParser(
        description="FPGA-Litho Unified Verification Runner"
    )

    parser.add_argument(
        "--config",
        type=str,
        default="input/config/golden_original.json",
        help="Config file path",
    )

    parser.add_argument(
        "--output", type=str, default="output/verification", help="Output directory"
    )

    parser.add_argument("--debug", action="store_true", help="Enable debug output")

    parser.add_argument("--quiet", action="store_true", help="Minimal output")

    parser.add_argument(
        "--compile-only", action="store_true", help="Compile only, don't run"
    )

    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="Keep existing output directory (default: clean before run)",
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="Force clean output directory (same as default behavior)",
    )

    args = parser.parse_args()

    # 解析路径
    config_path = PROJECT_ROOT / args.config
    output_dir = PROJECT_ROOT / args.output

    verbose = 2 if args.debug else (0 if args.quiet else 1)

    # 默认清理输出目录（除非指定 --no-clean）
    if not args.no_clean and output_dir.exists():
        print(f"[CLEAN] Removing existing output directory: {output_dir}")
        shutil.rmtree(output_dir)

    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 50)
    print("  FPGA-Litho Verification")
    print("=" * 50)

    # 编译
    if not compile_litho():
        return 1

    if args.compile_only:
        print("[DONE] Compilation complete.")
        return 0

    # 运行
    success = run_litho(config_path, output_dir, verbose)
    if success:
        success = analyze_results(output_dir)

    # 总结
    print("\n" + "=" * 50)
    if success:
        print("[SUCCESS] Verification passed!")
        print(f"Results: {output_dir}")
    else:
        print("[FAILED] Verification failed!")
    print("=" * 50)

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
