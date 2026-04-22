#!/usr/bin/env python3
"""
完整信任链条自动化验证脚本

用途：
  - 一站式验证板级验证数据注入的所有信任环节
  - 覆盖：BIN→HEX转换、DDR写入、HLS输出

信任链条：
  BIN → HEX转换 → DDR写入 → HLS读取 → 计算输出
     ↓           ↓          ↓          ↓
   格式正确？   效果一致？   格式匹配？   输出正确？

验证流程：
  1. 环节1：BIN→HEX转换正确性验证（Python脚本）
  2. 环节2：DDR写入效果一致性验证（需在Vivado中运行TCL）
  3. 环节3：HLS计算输出正确性验证（Python脚本）

使用方法：
  python verify_trust_chain.py --config <config_file>

示例：
  python verify_trust_chain.py --config ../board_validation_config.json

注意：
  - 环节2需要在Vivado Tcl Console中手动运行 verify_ddr_write.tcl
  - 环节3需要先生成DDR回读文件（通过TCL脚本）
"""

import argparse
import json
import os
import sys
import subprocess

# 添加脚本目录到Python路径
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(SCRIPT_DIR)


def run_verification_step(step_name, script_name, args_list):
    """运行单个验证步骤"""

    print(f"\n{'='*80}")
    print(f"验证环节: {step_name}")
    print(f"{'='*80}")

    # 构建命令
    cmd = ["python", os.path.join(SCRIPT_DIR, script_name)] + args_list

    # 执行命令
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

        # 打印输出
        print(result.stdout)
        if result.stderr:
            print(f"警告: {result.stderr}")

        # 检查结果
        if result.returncode == 0:
            print(f"✅ {step_name}验证成功")
            return True
        else:
            print(f"❌ {step_name}验证失败 (返回码: {result.returncode})")
            return False

    except subprocess.TimeoutExpired:
        print(f"❌ {step_name}验证超时")
        return False
    except Exception as e:
        print(f"❌ {step_name}验证异常: {e}")
        return False


def verify_trust_chain(config_path):
    """完整信任链条验证"""

    # 步骤0：加载配置
    print(f"加载配置文件: {config_path}")

    if not os.path.exists(config_path):
        print(f"错误: 配置文件不存在 {config_path}")
        return False

    with open(config_path, "r") as f:
        config = json.load(f)

    # 配置参数
    bin_files = config.get("bin_files", {})
    golden_file = config.get("golden_file", "")
    hls_output_len = config.get("hls_output_len", 289)

    print(f"配置参数:")
    print(f"  BIN文件: {bin_files}")
    print(f"  Golden文件: {golden_file}")
    print(f"  HLS输出长度: {hls_output_len}")

    # 验证结果汇总
    results = {"环节1_BIN_HEX": False, "环节2_DDR写入": False, "环节3_HLS输出": False}

    # ========================================================================
    # 环节1：BIN→HEX转换正确性验证
    # ========================================================================
    print(f"\n{'='*80}")
    print(f"环节1：BIN→HEX转换正确性验证")
    print(f"{'='*80}")

    # 验证所有BIN文件
    for bin_name, bin_path in bin_files.items():
        print(f"\n验证 {bin_name}: {bin_path}")

        if not os.path.exists(bin_path):
            print(f"⚠️  文件不存在: {bin_path}")
            continue

        # 运行环节1验证
        step_name = f"BIN→HEX ({bin_name})"
        success = run_verification_step(
            step_name, "verify_bin_hex_conversion.py", ["--input", bin_path]
        )

        if success:
            results["环节1_BIN_HEX"] = True
        else:
            results["环节1_BIN_HEX"] = False
            break  # 任一文件失败则停止

    # ========================================================================
    # 环节2：DDR写入效果一致性验证
    # ========================================================================
    print(f"\n{'='*80}")
    print(f"环节2：DDR写入效果一致性验证")
    print(f"{'='*80}")

    print(f"\n⚠️  此环节需要在Vivado Tcl Console中手动运行：")
    print(f"  1. 打开Vivado Hardware Manager")
    print(f"  2. 连接JTAG并编程FPGA")
    print(f"  3. 在Tcl Console中运行: source verify_ddr_write.tcl")
    print(f"  4. 检查输出结果是否为 ✅ 验证通过")

    # 等待用户确认
    print(f"\n请在Vivado中完成环节2验证后，输入结果：")
    user_input = input("环节2验证结果 (pass/fail/skip): ").strip().lower()

    if user_input == "pass":
        results["环节2_DDR写入"] = True
        print(f"✅ 用户确认：环节2验证通过")
    elif user_input == "skip":
        results["环节2_DDR写入"] = False
        print(f"⚠️  用户跳过：环节2验证未执行")
    else:
        results["环节2_DDR写入"] = False
        print(f"❌ 用户确认：环节2验证失败")

    # ========================================================================
    # 环节3：HLS计算输出正确性验证
    # ========================================================================
    print(f"\n{'='*80}")
    print(f"环节3：HLS计算输出正确性验证")
    print(f"{'='*80}")

    # 检查Golden文件
    if not os.path.exists(golden_file):
        print(f"⚠️  Golden文件不存在: {golden_file}")
        print(f"  跳过环节3验证")
        results["环节3_HLS输出"] = False
    else:
        # 检查DDR回读文件
        ddr_read_file = os.path.join(SCRIPT_DIR, "ddr_read_output.txt")

        if not os.path.exists(ddr_read_file):
            print(f"⚠️  DDR回读文件不存在: {ddr_read_file}")
            print(f"  请先在Vivado中运行DDR回读脚本，生成 {ddr_read_file}")
            print(f"  跳过环节3验证")
            results["环节3_HLS输出"] = False
        else:
            # 运行环节3验证
            step_name = "HLS输出验证"
            success = run_verification_step(
                step_name,
                "verify_hls_output.py",
                ["--golden", golden_file, "--hls_output_len", str(hls_output_len)],
            )

            results["环节3_HLS输出"] = success

    # ========================================================================
    # 验证结果汇总
    # ========================================================================
    print(f"\n{'='*80}")
    print(f"验证结果汇总")
    print(f"{'='*80}")

    for step_name, success in results.items():
        status = "✅ 通过" if success else "❌ 失败"
        print(f"  {step_name}: {status}")

    # 综合判断
    all_pass = all(results.values())

    if all_pass:
        print(f"\n✅ 信任链条完整验证通过")
        print(f"  数据注入流程可靠，可进行板级验证")
    else:
        print(f"\n❌ 信任链条验证存在失败环节")
        print(f"  请检查失败环节并修复问题")

    return all_pass


def main():
    parser = argparse.ArgumentParser(description="完整信任链条自动化验证")
    parser.add_argument(
        "--config", default="board_validation_config.json", help="配置文件路径"
    )
    args = parser.parse_args()

    # 执行验证
    success = verify_trust_chain(args.config)

    if success:
        print(f"\n验证完成：信任链条完整验证通过 ✅")
    else:
        print(f"\n验证完成：信任链条验证失败 ❌")


if __name__ == "__main__":
    main()
