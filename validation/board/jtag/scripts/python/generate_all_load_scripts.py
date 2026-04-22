#!/usr/bin/env python3
"""
生成所有 SOCS HLS 输入数据的 TCL 加载脚本

用途：
  - 批量生成 mskf_r/i, scales, kernels 的 TCL DDR 加载脚本
  - 用于 Vivado JTAG-to-AXI 数据加载
"""

import os
import sys

# 添加当前目录到路径，以便导入 bin_to_hex_for_ddr
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bin_to_hex_for_ddr import generate_tcl_script, read_bin_file

# 数据路径配置
DATA_DIR = "e:/fpga-litho-accel/source/SOCS_HLS/data"
OUTPUT_DIR = "e:/fpga-litho-accel/validation/board/jtag"

# DDR 地址映射
DDR_ADDRESS_MAP = {
    "mskf_r": 0x40000000,  # 512×512 floats
    "mskf_i": 0x42000000,  # 512×512 floats
    "scales": 0x44000000,  # 10 floats
    "krn_r": 0x44400000,  # 10×81 floats
    "krn_i": 0x44800000,  # 10×81 floats
}


def generate_all_scripts():
    """生成所有输入数据的 TCL 加载脚本"""

    print("=== 生成 SOCS HLS 输入数据加载脚本 ===")

    # 1. mskf_r
    print("\n>>> 生成 mskf_r 加载脚本...")
    mskf_r_path = os.path.join(DATA_DIR, "mskf_r.bin")
    mskf_r_out = os.path.join(OUTPUT_DIR, "load_mskf_r.tcl")
    floats = read_bin_file(mskf_r_path)
    generate_tcl_script(floats, mskf_r_out, DDR_ADDRESS_MAP["mskf_r"], "mskf_r_data")
    print(f"    ✅ {mskf_r_out}: {len(floats)} floats")

    # 2. mskf_i
    print("\n>>> 生成 mskf_i 加载脚本...")
    mskf_i_path = os.path.join(DATA_DIR, "mskf_i.bin")
    mskf_i_out = os.path.join(OUTPUT_DIR, "load_mskf_i.tcl")
    floats = read_bin_file(mskf_i_path)
    generate_tcl_script(floats, mskf_i_out, DDR_ADDRESS_MAP["mskf_i"], "mskf_i_data")
    print(f"    ✅ {mskf_i_out}: {len(floats)} floats")

    # 3. scales
    print("\n>>> 生成 scales 加载脚本...")
    scales_path = os.path.join(DATA_DIR, "scales.bin")
    scales_out = os.path.join(OUTPUT_DIR, "load_scales.tcl")
    floats = read_bin_file(scales_path)
    generate_tcl_script(floats, scales_out, DDR_ADDRESS_MAP["scales"], "scales_data")
    print(f"    ✅ {scales_out}: {len(floats)} floats")

    # 4. kernels (10个)
    print("\n>>> 生成 kernels 加载脚本...")
    kernels_dir = os.path.join(DATA_DIR, "kernels")

    # 合并所有 kernels 到单个文件
    krn_r_all = []
    krn_i_all = []

    for k in range(10):
        krn_r_path = os.path.join(kernels_dir, f"krn_{k}_r.bin")
        krn_i_path = os.path.join(kernels_dir, f"krn_{k}_i.bin")

        krn_r_all.extend(read_bin_file(krn_r_path))
        krn_i_all.extend(read_bin_file(krn_i_path))

    # 生成合并的 kernel 加载脚本
    krn_r_out = os.path.join(OUTPUT_DIR, "load_krn_r.tcl")
    krn_i_out = os.path.join(OUTPUT_DIR, "load_krn_i.tcl")

    generate_tcl_script(krn_r_all, krn_r_out, DDR_ADDRESS_MAP["krn_r"], "krn_r_data")
    generate_tcl_script(krn_i_all, krn_i_out, DDR_ADDRESS_MAP["krn_i"], "krn_i_data")

    print(f"    ✅ {krn_r_out}: {len(krn_r_all)} floats (10 kernels)")
    print(f"    ✅ {krn_i_out}: {len(krn_i_all)} floats (10 kernels)")

    print("\n=== 所有脚本生成完成 ===")
    print("\n>>> 使用方法：")
    print("    在 Vivado Tcl Console 中依次执行：")
    print("    cd e:/fpga-litho-accel/validation/board/jtag")
    print("    source load_all_data.tcl")


if __name__ == "__main__":
    generate_all_scripts()
