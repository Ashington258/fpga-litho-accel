#!/usr/bin/env python3
"""
将 Golden 数据 .bin 文件转换为 TCL DDR 写入脚本

用途：
  - 将 output/verification/*.bin 转换为 TCL hex 格式
  - 生成的脚本可在 Vivado Tcl Console 中直接运行
  - 用于 JTAG-to-AXI DDR 数据加载

使用方法：
  python bin_to_hex_for_ddr.py --input <bin_file> --output <tcl_file> --address <hex_addr>

示例：
  python bin_to_hex_for_ddr.py --input mskf_r.bin --output mskf_r.tcl --address 0x40000000
"""

import argparse
import struct
import os

# Vivado JTAG-to-AXI burst 限制
MAX_BURST_LEN = 128  # 每次写入最多 128 个 32-bit words


def float_to_hex(value):
    """将 float 值转换为 IEEE 754 hex 格式"""
    # pack as float (32-bit), unpack as unsigned int
    packed = struct.pack(">f", value)  # big-endian
    hex_val = struct.unpack(">I", packed)[0]
    return f"{hex_val:08X}"


def read_bin_file(bin_path):
    """读取 .bin 文件，返回 float32 数组"""
    floats = []
    with open(bin_path, "rb") as f:
        while True:
            data = f.read(4)  # 4 bytes = 1 float32
            if not data:
                break
            # 假设是 little-endian (Python numpy 默认)
            val = struct.unpack("<f", data)[0]
            floats.append(val)
    return floats


def generate_tcl_script(floats, output_path, base_address, var_name="data"):
    """生成 TCL DDR 写入脚本"""

    total_words = len(floats)
    num_batches = (total_words + MAX_BURST_LEN - 1) // MAX_BURST_LEN

    tcl_content = []

    # 添加脚本头
    tcl_content.append(
        f"# =============================================================================="
    )
    tcl_content.append(f"# DDR 数据加载脚本 - {var_name}")
    tcl_content.append(f"# 数据量: {total_words} floats ({total_words * 4} bytes)")
    tcl_content.append(f"# 分批数: {num_batches} batches (每批 {MAX_BURST_LEN} words)")
    tcl_content.append(f"# DDR地址: 0x{base_address:08X}")
    tcl_content.append(
        f"# =============================================================================="
    )
    tcl_content.append("")
    tcl_content.append(f'puts "\\n>>> 开始加载 {var_name} ({total_words} floats)..."')
    tcl_content.append("")
    tcl_content.append(f"set axi_if [get_hw_axis hw_axi_1]")
    tcl_content.append(f"set BASE_ADDR 0x{base_address:08X}")
    tcl_content.append("")

    # 分批生成写入命令
    for batch_idx in range(num_batches):
        start_idx = batch_idx * MAX_BURST_LEN
        end_idx = min(start_idx + MAX_BURST_LEN, total_words)
        batch_floats = floats[start_idx:end_idx]

        # 构建 hex 数据字符串（用下划线分隔）
        hex_values = [float_to_hex(v) for v in batch_floats]
        hex_string = "_".join(hex_values)

        # 计算地址
        batch_addr = base_address + start_idx * 4

        # 写入 TCL 命令
        tcl_content.append(f"# Batch {batch_idx}: {len(batch_floats)} words")
        tcl_content.append(
            f'set addr_{batch_idx} [format "0x%08X" [expr {{$BASE_ADDR + {batch_idx * MAX_BURST_LEN * 4}}}]]'
        )
        tcl_content.append(f'set data_{batch_idx} "{hex_string}"')
        tcl_content.append(
            f"catch {{delete_hw_axi_txn [get_hw_axi_txns wr_{var_name}_{batch_idx}]}}"
        )
        tcl_content.append(
            f"create_hw_axi_txn wr_{var_name}_{batch_idx} $axi_if -address $addr_{batch_idx} -data $data_{batch_idx} -len {len(batch_floats)} -type write"
        )
        tcl_content.append(f"run_hw_axi [get_hw_axi_txns wr_{var_name}_{batch_idx}]")

        # 每 10 批打印进度
        if batch_idx % 10 == 0:
            tcl_content.append(f'puts "进度: Batch {batch_idx} / {num_batches}"')

        tcl_content.append("")

    # 添加验证读取
    tcl_content.append(f"# 验证写入")
    tcl_content.append(
        f"catch {{delete_hw_axi_txn [get_hw_axi_txns verify_{var_name}]}}"
    )
    tcl_content.append(
        f"create_hw_axi_txn verify_{var_name} $axi_if -address $BASE_ADDR -len 4 -type read"
    )
    tcl_content.append(f"run_hw_axi [get_hw_axi_txns verify_{var_name}]")
    tcl_content.append(
        f"set verify_data [get_property DATA [get_hw_axi_txns verify_{var_name}]]"
    )
    tcl_content.append(f'puts "验证读取: $verify_data"')
    tcl_content.append("")
    tcl_content.append(f'puts "✅ {var_name} 加载完成"')

    # 写入文件
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(tcl_content))

    print(f"生成 TCL 脚本: {output_path}")
    print(f"  数据量: {total_words} floats")
    print(f"  分批数: {num_batches}")
    print(f"  文件大小: {os.path.getsize(output_path)} bytes")


def main():
    parser = argparse.ArgumentParser(description="将 BIN 文件转换为 TCL DDR 写入脚本")
    parser.add_argument("--input", required=True, help="输入 BIN 文件路径")
    parser.add_argument("--output", required=True, help="输出 TCL 脚本路径")
    parser.add_argument(
        "--address", required=True, help="DDR 基地址 (hex 格式，如 0x40000000)"
    )
    parser.add_argument("--var-name", default="data", help="TCL 变量名称")

    args = parser.parse_args()

    # 解析地址
    base_addr = int(args.address, 16)

    # 读取 BIN 文件
    print(f"读取 BIN 文件: {args.input}")
    floats = read_bin_file(args.input)

    if len(floats) == 0:
        print("错误: BIN 文件为空或格式不正确")
        return

    # 生成 TCL 脚本
    generate_tcl_script(floats, args.output, base_addr, args.var_name)


if __name__ == "__main__":
    main()
