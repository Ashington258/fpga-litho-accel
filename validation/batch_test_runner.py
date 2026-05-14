#!/usr/bin/env python3
"""
FPGA-Litho 批量测试执行脚本
==========================

批量执行多个测试用例，支持配置文件驱动的测试套件。

Usage:
    python batch_test_runner.py --config input/config/Different_mask_tests.json
    python batch_test_runner.py --config input/config/Different_resolution_tests.json --dry-run
    python batch_test_runner.py --config input/config/Different_mask_tests.json --output-base output

Features:
    - 从JSON配置文件读取测试用例
    - 为每个测试用例生成临时配置文件
    - 调用 run_verification.py 执行测试
    - 组织输出到独立子目录
    - 生成测试汇总报告（JSON格式）
"""

import os
import sys
import json
import argparse
import shutil
import time
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional
import subprocess

# 项目根目录
PROJECT_ROOT = Path(__file__).resolve().parent.parent

# 导入 run_verification 模块
sys.path.insert(0, str(PROJECT_ROOT / "validation" / "golden"))
from run_verification import compile_litho, run_litho, analyze_results


class BatchTestRunner:
    """批量测试执行器"""

    def __init__(
        self,
        config_path: Path,
        output_base: Path,
        dry_run: bool = False,
        verbose: int = 1,
        executable_name: str = "litho",
    ):
        self.config_path = config_path
        self.output_base = output_base
        self.dry_run = dry_run
        self.verbose = verbose
        self.executable_name = executable_name
        self.results = []

    def load_config(self) -> Dict[str, Any]:
        """加载批量测试配置文件"""
        with open(self.config_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def generate_test_config(
        self, test_case: Dict[str, Any], common_config: Dict[str, Any]
    ) -> Dict[str, Any]:
        """生成单个测试用例的完整配置"""
        # 从基础配置开始
        full_config = {
            "simulation": {
                "description": f"Test case: {test_case['testName']}",
                "version": "1.0",
            },
            "mask": test_case.get("mask", {}),
            "source": common_config.get("source", {}),
            "optics": common_config.get("optics", {}),
            "kernel": common_config.get("kernel", {}),
            "output": {
                "baseDir": "out",
                "kernelsDir": "out/kernels",
                "kernelsPngDir": "out/kernels/png",
                "imageFile": "out/image.bin",
                "imagePng": "out/image.png",
                "imageSegmentedPng": "out/image_s.png",
                "maskPng": "out/mask.png",
                "sourcePng": "out/source.png",
                "tccRealPng": "out/tcc_r.png",
                "tccImagPng": "out/tcc_i.png",
            },
        }
        return full_config

    def validate_test_case(self, test_case: Dict[str, Any]) -> List[str]:
        """验证测试用例配置的有效性"""
        errors = []

        # 检查必需字段
        if "testName" not in test_case:
            errors.append("Missing 'testName' field")
        
        # 检查mask配置
        if "mask" not in test_case:
            errors.append("Missing 'mask' configuration")
        else:
            mask_config = test_case["mask"]
            if "inputFile" not in mask_config:
                errors.append("Missing 'mask.inputFile' field")
            else:
                # 检查mask文件是否存在
                mask_path = PROJECT_ROOT / mask_config["inputFile"]
                if not mask_path.exists():
                    errors.append(f"Mask file not found: {mask_config['inputFile']}")

        return errors

    def run_single_test(
        self, test_case: Dict[str, Any], common_config: Dict[str, Any]
    ) -> Dict[str, Any]:
        """执行单个测试用例"""
        test_name = test_case["testName"]
        start_time = time.time()

        result = {
            "testName": test_name,
            "description": test_case.get("description", ""),
            "status": "pending",
            "startTime": datetime.now().isoformat(),
            "duration": 0.0,
            "outputDir": "",
            "errors": [],
        }

        # 验证测试用例
        validation_errors = self.validate_test_case(test_case)
        if validation_errors:
            result["status"] = "validation_failed"
            result["errors"] = validation_errors
            return result

        # 生成完整配置
        full_config = self.generate_test_config(test_case, common_config)

        # 创建输出目录
        test_output_dir = self.output_base / test_name
        result["outputDir"] = str(test_output_dir)

        if self.dry_run:
            result["status"] = "dry_run"
            print(f"[DRY-RUN] Would create: {test_output_dir}")
            print(f"[DRY-RUN] Config preview:")
            print(json.dumps(full_config, indent=2))
            return result

        # 创建输出目录
        test_output_dir.mkdir(parents=True, exist_ok=True)

        # 创建临时配置文件（直接放在 input/config/ 下，以便 litho.cpp 正确推导项目根目录）
        # litho.cpp: projectRoot = configDir.parent_path().parent_path()
        # 如果 configPath = input/config/config_T1.json，则 projectRoot = 项目根目录
        temp_config_path = PROJECT_ROOT / "input" / "config" / f"config_{test_name}.json"
        with open(temp_config_path, "w", encoding="utf-8") as f:
            json.dump(full_config, f, indent=4)

        print(f"\n{'='*60}")
        print(f"[TEST] Running: {test_name}")
        print(f"[TEST] Description: {test_case.get('description', 'N/A')}")
        print(f"[TEST] Output: {test_output_dir}")
        print(f"{'='*60}")

        try:
            # 执行测试
            success = run_litho(temp_config_path, test_output_dir, self.verbose, self.executable_name)

            if success:
                # 分析结果
                analyze_success = analyze_results(test_output_dir)
                result["status"] = "success" if analyze_success else "analysis_failed"
            else:
                result["status"] = "execution_failed"

        except Exception as e:
            result["status"] = "error"
            result["errors"].append(str(e))
            print(f"[ERROR] Test {test_name} failed: {e}")

        finally:
            # 记录执行时间
            result["duration"] = time.time() - start_time
            result["endTime"] = datetime.now().isoformat()

        return result

    def run_all_tests(self) -> Dict[str, Any]:
        """执行所有测试用例"""
        # 加载配置
        config = self.load_config()
        test_suite = config.get("testSuite", "unknown")
        test_cases = config.get("testCases", [])
        common_config = config.get("commonConfig", {})

        print(f"\n{'#'*60}")
        print(f"# Batch Test Suite: {test_suite}")
        print(f"# Total test cases: {len(test_cases)}")
        print(f"# Output base: {self.output_base}")
        print(f"# Dry run: {self.dry_run}")
        print(f"{'#'*60}\n")

        # 创建输出基础目录
        if not self.dry_run:
            self.output_base.mkdir(parents=True, exist_ok=True)

        # 执行所有测试用例
        for i, test_case in enumerate(test_cases, 1):
            print(f"\n[PROGRESS] Test {i}/{len(test_cases)}: {test_case.get('testName', 'unknown')}")
            result = self.run_single_test(test_case, common_config)
            self.results.append(result)

            # 打印中间结果
            status_emoji = "✓" if result["status"] == "success" else "✗"
            print(f"[RESULT] {status_emoji} {result['testName']}: {result['status']} ({result['duration']:.2f}s)")

        # 生成汇总报告
        summary = self.generate_summary(test_suite)

        return summary

    def generate_summary(self, test_suite: str) -> Dict[str, Any]:
        """生成测试汇总报告"""
        total = len(self.results)
        success_count = sum(1 for r in self.results if r["status"] == "success")
        failed_count = total - success_count

        summary = {
            "testSuite": test_suite,
            "timestamp": datetime.now().isoformat(),
            "summary": {
                "total": total,
                "success": success_count,
                "failed": failed_count,
                "successRate": f"{success_count/total*100:.1f}%" if total > 0 else "0%",
            },
            "totalDuration": sum(r["duration"] for r in self.results),
            "results": self.results,
        }

        # 打印汇总
        print(f"\n{'#'*60}")
        print(f"# Test Summary: {test_suite}")
        print(f"{'#'*60}")
        print(f"Total: {total}")
        print(f"Success: {success_count}")
        print(f"Failed: {failed_count}")
        print(f"Success Rate: {summary['summary']['successRate']}")
        print(f"Total Duration: {summary['totalDuration']:.2f}s")
        print(f"{'#'*60}\n")

        # 保存报告到JSON文件
        if not self.dry_run:
            report_path = self.output_base / "test_report.json"
            with open(report_path, "w", encoding="utf-8") as f:
                json.dump(summary, f, indent=2, ensure_ascii=False)
            print(f"[REPORT] Saved to: {report_path}")

        return summary


def main():
    parser = argparse.ArgumentParser(
        description="FPGA-Litho Batch Test Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Run Different_mask_tests suite
    python batch_test_runner.py --config input/config/Different_mask_tests.json

    # Dry run to preview test plan
    python batch_test_runner.py --config input/config/Different_mask_tests.json --dry-run

    # Specify custom output directory
    python batch_test_runner.py --config input/config/Different_mask_tests.json --output-base output/custom_tests
        """,
    )

    parser.add_argument(
        "--config",
        type=str,
        required=True,
        help="Path to batch test configuration JSON file",
    )

    parser.add_argument(
        "--output-base",
        type=str,
        default=None,
        help="Base directory for test outputs (default: auto-detect from config name)",
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview test plan without executing",
    )

    parser.add_argument(
        "--verbose",
        "-v",
        action="count",
        default=1,
        help="Increase verbosity (0=quiet, 1=normal, 2=debug)",
    )

    parser.add_argument(
        "--compile-only",
        action="store_true",
        help="Only compile litho binary, do not run tests",
    )

    parser.add_argument(
        "--executable",
        type=str,
        default="litho",
        choices=["litho", "litho_thread"],
        help="Executable to use (default: litho, litho_thread for multi-threaded)",
    )

    args = parser.parse_args()

    # 解析路径
    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = PROJECT_ROOT / config_path

    if not config_path.exists():
        print(f"[ERROR] Config file not found: {config_path}")
        sys.exit(1)

    # 自动推断输出目录
    if args.output_base:
        output_base = Path(args.output_base)
    else:
        # 从配置文件名推断
        config_name = config_path.stem  # e.g., "Different_mask_tests"
        output_base = PROJECT_ROOT / "output" / config_name

    if not output_base.is_absolute():
        output_base = PROJECT_ROOT / output_base

    # 仅编译模式
    if args.compile_only:
        print("[COMPILE-ONLY] Building litho binary...")
        success = compile_litho()
        sys.exit(0 if success else 1)

    # 创建执行器并运行
    runner = BatchTestRunner(
        config_path=config_path,
        output_base=output_base,
        dry_run=args.dry_run,
        verbose=args.verbose,
        executable_name=args.executable,
    )

    summary = runner.run_all_tests()

    # 返回退出码
    sys.exit(0 if summary["summary"]["failed"] == 0 else 1)


if __name__ == "__main__":
    main()
