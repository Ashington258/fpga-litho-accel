#!/usr/bin/env python3
"""
批量修复TCL脚本中的JTAG-to-AXI反向写入问题
在 create_hw_axi_txn 之前添加 [lreverse] 处理

使用方法：
    python fix_lreverse_batch.py <tcl_file> [--dry-run] [--verbose]

示例：
    python fix_lreverse_batch.py load_mskf_r.tcl --dry-run  # 预览修改
    python fix_lreverse_batch.py load_mskf_r.tcl            # 执行修复
"""

import re
import sys
import argparse
from pathlib import Path
from typing import List, Tuple


def find_batches(content: str) -> List[Tuple[int, str, str, int]]:
    """
    找到所有需要修复的batch块
    返回: [(batch_num, data_var, original_txn, len_value), ...]
    """
    pattern = r"""
# Batch (\d+): (\d+) words
set addr_\d+ \[format "0x%08X" \[expr \{\$BASE_ADDR \+ \d+\}\]\]
set data_(\d+) "([^"]+)"
(\s*)catch \{delete_hw_axi_txn \[get_hw_axi_txns wr_\w+_data_\d+\]\}
create_hw_axi_txn wr_\w+_data_\d+ \$axi_if -address \$addr_\d+ -data \$data_(\d+) -len (\d+) -type write
run_hw_axi \[get_hw_axi_txns wr_\w+_data_\d+\]
"""

    # 更灵活的模式匹配
    batch_pattern = re.compile(
        r"# Batch (\d+): (\d+) words.*?\n"
        r"set addr_\d+ .*?\n"
        r'set data_(\d+) "([^"]+)"\n'
        r"(catch \{delete_hw_axi_txn .*?\}\n"
        r"create_hw_axi_txn .*? -data \$data_\d+ -len (\d+) -type write\n"
        r"run_hw_axi .*?\n)",
        re.MULTILINE,
    )

    matches = []
    for m in batch_pattern.finditer(content):
        batch_num = int(m.group(1))
        data_var = m.group(3)
        data_content = m.group(4)
        txn_block = m.group(5)
        len_value = int(m.group(6))

        # 检查是否已经修复（是否包含lreverse）
        if "lreverse" in txn_block:
            continue

        matches.append(
            (batch_num, data_var, data_content, len_value, m.start(), m.end())
        )

    return matches


def generate_fix_block(
    data_var: str, len_value: int, txn_name: str, addr_var: str
) -> str:
    """
    生成修复后的TCL代码块
    """
    return f"""set {data_var}_list [split ${data_var} "_"]
set {data_var}_reversed [join [lreverse ${data_var}_list] "_"]
catch {{delete_hw_axi_txn [get_hw_axi_txns {txn_name}]}}
create_hw_axi_txn {txn_name} $axi_if -address ${addr_var} -data ${data_var}_reversed -len {len_value} -type write
run_hw_axi [get_hw_axi_txns {txn_name}]
"""


def fix_tcl_file(filepath: Path, dry_run: bool = False, verbose: bool = False) -> int:
    """
    修复TCL文件，返回修复的batch数量
    """
    content = filepath.read_text(encoding="utf-8")

    # 提取脚本名称（用于txn命名）
    script_name = filepath.stem.replace("load_", "")
    txn_prefix = f"wr_{script_name}_data"

    # 找到所有batch
    batches = find_batches(content)

    if not batches:
        print(f"✅ {filepath.name}: 无需修复（已全部修复或无batch）")
        return 0

    print(f"🔍 {filepath.name}: 发现 {len(batches)} 个batch需要修复")

    if dry_run:
        for batch_num, data_var, data_content, len_val, start, end in batches[:3]:
            print(f"   Batch {batch_num}: data_{data_var} ({len_val} words)")
            print(f"   数据片段: {data_content[:50]}...")
        if len(batches) > 3:
            print(f"   ... 还有 {len(batches) - 3} 个batch")
        return len(batches)

    # 执行修复
    fixed_content = content
    fixed_count = 0

    # 使用更精确的模式进行替换
    for batch_num, data_var, data_content, len_val, start, end in batches:
        if verbose:
            print(f"   修复 Batch {batch_num}...")

        # 找到txn块并替换
        old_pattern = re.compile(
            r"(set data_" + data_var + r' "[^"]+"\n)'
            r"(catch \{delete_hw_axi_txn \[get_hw_axi_txns "
            + txn_prefix
            + "_"
            + data_var
            + r"\]\}\n)"
            r"(create_hw_axi_txn "
            + txn_prefix
            + "_"
            + data_var
            + r" \$axi_if -address \$addr_"
            + data_var
            + r" -data \$data_"
            + data_var
            + r" -len "
            + str(len_val)
            + r" -type write\n)"
            r"(run_hw_axi \[get_hw_axi_txns " + txn_prefix + "_" + data_var + r"\]\n)",
            re.MULTILINE,
        )

        # 构建新的txn块
        new_txn = f"""set data_{data_var}_list [split $data_{data_var} "_"]
set data_{data_var}_reversed [join [lreverse $data_{data_var}_list] "_"]
catch {{delete_hw_axi_txn [get_hw_axi_txns {txn_prefix}_{data_var}]}}
create_hw_axi_txn {txn_prefix}_{data_var} $axi_if -address $addr_{data_var} -data $data_{data_var}_reversed -len {len_val} -type write
run_hw_axi [get_hw_axi_txns {txn_prefix}_{data_var}]
"""

        # 执行替换
        match = old_pattern.search(fixed_content)
        if match:
            # 只替换txn部分，保留set data行
            full_match_start = match.start()
            full_match_end = match.end()

            # 定位set data行结束位置
            set_data_end = match.group(1).endswith("\n")

            # 替换：保留set data行，替换后续txn块
            before = fixed_content[: match.start() + len(match.group(1))]
            after = fixed_content[match.end() :]
            fixed_content = before + new_txn + after
            fixed_count += 1

    if fixed_count > 0:
        filepath.write_text(fixed_content, encoding="utf-8")
        print(f"✅ {filepath.name}: 已修复 {fixed_count} 个batch")

    return fixed_count


def main():
    parser = argparse.ArgumentParser(
        description="批量修复TCL脚本中的JTAG-to-AXI反向写入问题"
    )
    parser.add_argument("files", nargs="+", help="要修复的TCL文件")
    parser.add_argument("--dry-run", action="store_true", help="仅预览，不修改文件")
    parser.add_argument("--verbose", "-v", action="store_true", help="显示详细信息")

    args = parser.parse_args()

    total_fixed = 0
    for filepath_str in args.files:
        filepath = Path(filepath_str)
        if not filepath.exists():
            print(f"❌ 文件不存在: {filepath}")
            continue

        fixed = fix_tcl_file(filepath, args.dry_run, args.verbose)
        total_fixed += fixed

    print(f"\n{'预览模式: ' if args.dry_run else ''}共发现/修复 {total_fixed} 个batch")


if __name__ == "__main__":
    main()
