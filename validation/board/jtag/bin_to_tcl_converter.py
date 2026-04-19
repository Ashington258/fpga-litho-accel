#!/usr/bin/env python3
"""
数据转换脚本：将 BIN 文件转换为 TCL hex 格式
用于 SOCS HLS IP 板级验证

使用方法：
    python generate_tcl_data.py [--output-dir tcl/data]

输出：
    - socs_hls_data.tcl  : 包含所有数据的 TCL 变量定义
    - socs_hls_data_partial.tcl : 分批写入的大数据（mskf_r/i）

数据来源：output/verification_original/
"""

import os
import sys
import struct
import argparse
from pathlib import Path

# ============================================================================
# 配置参数
# ============================================================================

# 默认输入目录（使用已有的 verification_original 数据）
DEFAULT_INPUT_DIR = Path("e:/fpga-litho-accel/output/verification_original")

# 输出文件配置
OUTPUT_TCL_FILE = "socs_hls_data.tcl"
OUTPUT_PARTIAL_TCL_FILE = "socs_hls_data_partial.tcl"

# Vivado JTAG-to-AXI burst 限制
MAX_BURST_LEN = 128  # 每批次最大 word 数

# ============================================================================
# 数据读取函数
# ============================================================================

def read_bin_file(filepath: Path) -> list:
    """读取 BIN 文件，返回 float32 列表"""
    with open(filepath, 'rb') as f:
        data = f.read()
    
    num_floats = len(data) // 4
    floats = struct.unpack(f'{num_floats}f', data)
    return list(floats)

def float_to_hex(value: float) -> str:
    """将 float32 转换为 8 位 hex 字符串"""
    # 先打包为 float32 bytes，再转换为 hex
    packed = struct.pack('f', value)
    hex_str = packed.hex().upper()
    return hex_str

def floats_to_hex_list(floats: list) -> list:
    """将 float 列表转换为 hex 字符串列表"""
    return [float_to_hex(f) for f in floats]

def hex_list_to_tcl_string(hex_list: list, indent="    ") -> str:
    """将 hex 列表转换为 TCL 格式字符串"""
    # 每 8 个元素一行，便于阅读
    lines = []
    for i in range(0, len(hex_list), 8):
        chunk = hex_list[i:i+8]
        lines.append(indent + " ".join(chunk))
    
    return "\n".join(lines)

# ============================================================================
# 数据分析函数
# ============================================================================

def analyze_bin_file(filepath: Path) -> dict:
    """分析 BIN 文件基本信息"""
    if not filepath.exists():
        return {"exists": False, "path": str(filepath)}
    
    floats = read_bin_file(filepath)
    return {
        "exists": True,
        "path": str(filepath),
        "size_bytes": filepath.stat().st_size,
        "num_floats": len(floats),
        "min": min(floats) if floats else 0,
        "max": max(floats) if floats else 0,
        "first_5": floats[:5] if floats else []
    }

# ============================================================================
# TCL 数据生成函数
# ============================================================================

def generate_small_data_tcl(name: str, hex_list: list, comment: str) -> str:
    """生成小型数据的 TCL 变量定义（直接写入）"""
    hex_str = hex_list_to_tcl_string(hex_list)
    return f"""
# {comment}
# 数据量: {len(hex_list)} floats
set {name} {{
{hex_str}
}}
"""

def generate_batch_data_tcl(name: str, hex_list: list, comment: str, base_addr: int) -> str:
    """生成大型数据的 TCL 分批写入定义"""
    total_len = len(hex_list)
    batch_count = (total_len + MAX_BURST_LEN - 1) // MAX_BURST_LEN
    
    content = f"""
# {comment}
# 数据量: {total_len} floats ({total_len * 4} bytes)
# 分批写入: {batch_count} batches (max {MAX_BURST_LEN} per batch)
# DDR 基地址: 0x{base_addr:08X}

# ===== 整体数据定义（用于验证） =====
set {name}_full {{
{hex_list_to_tcl_string(hex_list[:min(256, len(hex_list))])}  ;# 显示前 256 个
}}

# ===== 分批数据定义 =====
"""
    
    for batch_idx in range(batch_count):
        start_idx = batch_idx * MAX_BURST_LEN
        end_idx = min(start_idx + MAX_BURST_LEN, total_len)
        batch_data = hex_list[start_idx:end_idx]
        batch_addr = base_addr + start_idx * 4
        
        batch_hex_str = hex_list_to_tcl_string(batch_data)
        content += f"""
set {name}_batch_{batch_idx} {{
{batch_hex_str}
}}
# batch_{batch_idx}: {len(batch_data)} floats @ 0x{batch_addr:08X}
"""
    
    return content

def generate_kernel_data_tcl(kernel_dir: Path, num_kernels: int = 10) -> str:
    """生成 kernel 数据的 TCL 定义"""
    content = """
# ============================================================================
# SOCS Kernel 数据 (10 个 kernel，每个 9×9 = 81 floats)
# ============================================================================
"""
    
    for k in range(num_kernels):
        krn_r_path = kernel_dir / f"krn_{k}_r.bin"
        krn_i_path = kernel_dir / f"krn_{k}_i.bin"
        
        if krn_r_path.exists():
            krn_r_hex = floats_to_hex_list(read_bin_file(krn_r_path))
            content += generate_small_data_tcl(f"krn_{k}_r", krn_r_hex, f"Kernel {k} 实部 (9×9)")
        
        if krn_i_path.exists():
            krn_i_hex = floats_to_hex_list(read_bin_file(krn_i_path))
            content += generate_small_data_tcl(f"krn_{k}_i", krn_i_hex, f"Kernel {k} 虚部 (9×9)")
    
    return content

# ============================================================================
# 主生成函数
# ============================================================================

def generate_tcl_data_file(input_dir: Path, output_dir: Path) -> dict:
    """生成完整的 TCL 数据文件"""
    
    print("=" * 60)
    print("SOCS HLS 数据转换脚本")
    print("=" * 60)
    print(f"输入目录: {input_dir}")
    print(f"输出目录: {output_dir}")
    print()
    
    # 检查输入目录
    if not input_dir.exists():
        print(f"[ERROR] 输入目录不存在: {input_dir}")
        return {"success": False, "error": "Input directory not found"}
    
    # 创建输出目录
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # ========== 分析数据文件 ==========
    print("[ANALYSIS] 检查数据文件...")
    
    data_files = {
        "mskf_r": input_dir / "mskf_r.bin",
        "mskf_i": input_dir / "mskf_i.bin",
        "scales": input_dir / "scales.bin",
        "tmpImgp_pad32": input_dir / "tmpImgp_pad32.bin",
        "aerial_image": input_dir / "aerial_image_socs_kernel.bin",
    }
    
    analysis_results = {}
    for name, path in data_files.items():
        analysis_results[name] = analyze_bin_file(path)
        if analysis_results[name]["exists"]:
            print(f"  ✓ {name}: {analysis_results[name]['num_floats']} floats")
        else:
            print(f"  ✗ {name}: MISSING")
    
    # Kernel 目录
    kernel_dir = input_dir / "kernels"
    if kernel_dir.exists():
        print(f"  ✓ kernels/: 找到 kernel 目录")
        for k in range(10):
            krn_r = kernel_dir / f"krn_{k}_r.bin"
            krn_i = kernel_dir / f"krn_{k}_i.bin"
            if krn_r.exists() and krn_i.exists():
                print(f"    ✓ krn_{k}_r/i.bin")
    else:
        print(f"  ✗ kernels/: MISSING")
    
    print()
    
    # ========== 生成 TCL 文件 ==========
    print("[GENERATE] 生成 TCL 数据文件...")
    
    # DDR 地址映射（从 AddressSegments.csv 和 TCL 脚本）
    DDR_ADDR = {
        "mskf_r": 0x40000000,
        "mskf_i": 0x42000000,
        "scales": 0x44000000,
        "krn_r":  0x44400000,  # 所有 kernel 实部合并
        "krn_i":  0x44800000,  # 所有 kernel 虚部合并
        "output": 0x44840000,
    }
    
    # 生成主 TCL 文件
    main_tcl_content = """
# ==============================================================================
# SOCS HLS IP 测试数据定义
# 由 generate_tcl_data.py 自动生成
# 数据来源: output/verification_original/
# 
# 使用方法：
#   source socs_hls_data.tcl
#   source socs_hls_data_partial.tcl  ;# 加载分批数据
# ==============================================================================

"""

    # 1. scales.bin (10 floats) - 小数据，直接写入
    if data_files["scales"].exists():
        scales_hex = floats_to_hex_list(read_bin_file(data_files["scales"]))
        main_tcl_content += generate_small_data_tcl(
            "scales_data", scales_hex,
            "TCC 特征值 (scales.bin, 10 floats)"
        )
        print(f"  ✓ scales: {len(scales_hex)} floats")
    
    # 2. tmpImgp_pad32.bin (32×32 = 1024 floats) - HLS 验证参考
    if data_files["tmpImgp_pad32"].exists():
        tmp_hex = floats_to_hex_list(read_bin_file(data_files["tmpImgp_pad32"]))
        main_tcl_content += generate_small_data_tcl(
            "tmpImgp_pad32_data", tmp_hex,
            "HLS 中间输出参考 (tmpImgp_pad32.bin, 32×32 floats)"
        )
        print(f"  ✓ tmpImgp_pad32: {len(tmp_hex)} floats")
    
    # 3. aerial_image.bin (512×512 floats) - 最终输出参考（仅前部分）
    if data_files["aerial_image"].exists():
        aerial_floats = read_bin_file(data_files["aerial_image"])
        # 只保存前 256 个作为样本参考
        aerial_hex_sample = floats_to_hex_list(aerial_floats[:256])
        main_tcl_content += generate_small_data_tcl(
            "aerial_image_sample", aerial_hex_sample,
            "最终空中像参考 (aerial_image_socs_kernel.bin 前 256 floats)"
        )
        print(f"  ✓ aerial_image: {len(aerial_floats)} floats (保存前 256 作为样本)")
    
    # 4. Kernel 数据
    if kernel_dir.exists():
        main_tcl_content += generate_kernel_data_tcl(kernel_dir)
        print(f"  ✓ kernels: 生成 10 个 kernel 数据")
    
    # 生成分批数据文件（大型数据）
    partial_tcl_content = """
# ==============================================================================
# SOCS HLS IP 分批写入数据定义
# 用于 JTAG-to-AXI Master 分批写入 DDR
# MAX_BURST_LEN = 128 (Vivado 限制)
# ==============================================================================

"""

    # 5. mskf_r.bin (512×512 = 262144 floats) - 分批写入
    if data_files["mskf_r"].exists():
        mskf_r_hex = floats_to_hex_list(read_bin_file(data_files["mskf_r"]))
        partial_tcl_content += generate_batch_data_tcl(
            "mskf_r", mskf_r_hex,
            "Mask 频域实部 (mskf_r.bin, 512×512 floats)",
            DDR_ADDR["mskf_r"]
        )
        print(f"  ✓ mskf_r: {len(mskf_r_hex)} floats ({len(mskf_r_hex)//MAX_BURST_LEN + 1} batches)")
    
    # 6. mskf_i.bin (512×512 = 262144 floats) - 分批写入
    if data_files["mskf_i"].exists():
        mskf_i_hex = floats_to_hex_list(read_bin_file(data_files["mskf_i"]))
        partial_tcl_content += generate_batch_data_tcl(
            "mskf_i", mskf_i_hex,
            "Mask 频域虚部 (mskf_i.bin, 512×512 floats)",
            DDR_ADDR["mskf_i"]
        )
        print(f"  ✓ mskf_i: {len(mskf_i_hex)} floats ({len(mskf_i_hex)//MAX_BURST_LEN + 1} batches)")
    
    # ========== 写入文件 ==========
    main_tcl_path = output_dir / OUTPUT_TCL_FILE
    partial_tcl_path = output_dir / OUTPUT_PARTIAL_TCL_FILE
    
    with open(main_tcl_path, 'w', encoding='utf-8') as f:
        f.write(main_tcl_content)
    print(f"\n[OUTPUT] 主数据文件: {main_tcl_path}")
    
    with open(partial_tcl_path, 'w', encoding='utf-8') as f:
        f.write(partial_tcl_content)
    print(f"[OUTPUT] 分批数据文件: {partial_tcl_path}")
    
    # ========== 生成使用说明 ==========
    # 计算 mskf 批次数
    mskf_r_batches = len(mskf_r_hex)//MAX_BURST_LEN + 1 if data_files["mskf_r"].exists() else 0
    
    usage_content = """
# ==============================================================================
# 数据使用说明
# ==============================================================================

# 在 Vivado Tcl Console 中执行：
#   source """ + main_tcl_path.name + """
#   source """ + partial_tcl_path.name + """

# 写入 scales 到 DDR:
#   axi_write_data $axi_if 0x44000000 $scales_data "Write scales"

# 写入 kernel 数据:
#   for {{set k 0}} {{$k < 10}} {{incr k}} {{
#       axi_write_data $axi_if [expr 0x44400000 + $k*81*4] $krn_$k_r_data "Write krn_$k_r"
#       axi_write_data $axi_if [expr 0x44800000 + $k*81*4] $krn_$k_i_data "Write krn_$k_i"
#   }}

# 分批写入 mskf_r (需要循环):
#   set mskf_r_batches """ + str(mskf_r_batches) + """
#   for {{set b 0}} {{$b < $mskf_r_batches}} {{incr b}} {{
#       axi_write_data $axi_if [expr 0x40000000 + $b*128*4] $mskf_r_batch_$b "Write mskf_r batch $b"
#   }}

"""
    
    with open(output_dir / "data_usage.tcl", 'w', encoding='utf-8') as f:
        f.write(usage_content)
    print(f"[OUTPUT] 使用说明: {output_dir / 'data_usage.tcl'}")
    
    # ========== 生成统计报告 ==========
    report = {
        "success": True,
        "input_dir": str(input_dir),
        "output_dir": str(output_dir),
        "files_generated": [
            str(main_tcl_path),
            str(partial_tcl_path),
            str(output_dir / "data_usage.tcl")
        ],
        "data_stats": analysis_results
    }
    
    print("\n" + "=" * 60)
    print("生成完成!")
    print("=" * 60)
    
    return report

# ============================================================================
# 命令行入口
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="将 BIN 文件转换为 TCL hex 格式")
    parser.add_argument("--input-dir", type=str, default=str(DEFAULT_INPUT_DIR),
                        help="输入 BIN 文件目录")
    parser.add_argument("--output-dir", type=str, default="tcl/data",
                        help="输出 TCL 文件目录")
    
    args = parser.parse_args()
    
    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    
    # 如果 output_dir 不是绝对路径，相对于项目根目录
    if not output_dir.is_absolute():
        project_root = Path(__file__).parent.parent
        output_dir = project_root / output_dir
    
    result = generate_tcl_data_file(input_dir, output_dir)
    
    if not result["success"]:
        sys.exit(1)

if __name__ == "__main__":
    main()