#!/usr/bin/env python3
"""
SOCS_HLS目录整理脚本
将散落的文档移动到doc目录，保持目录整洁

Author: FPGA-Litho Project
Version: 1.0
"""

import os
import shutil
from pathlib import Path
import sys

# =====================================================================
# 配置
# =====================================================================

WORKSPACE_ROOT = Path("e:/fpga-litho-accel")
SOCS_HLS_DIR = WORKSPACE_ROOT / "source" / "SOCS_HLS"
DOC_DIR = SOCS_HLS_DIR / "doc"

# 需要移动的文件模式
FILE_PATTERNS = [
    # 项目状态文件
    "PROJECT_*.txt",
    "PROJECT_*.md",
    "FINAL_*.txt",
    "FINAL_*.md",
    "PERFORMANCE_*.txt",
    "PERFORMANCE_*.md",
    "CORRECTED_*.txt",
    "CORRECTED_*.md",
    # 验证报告
    "VERIFICATION_*.txt",
    "VERIFICATION_*.md",
    # 其他文档
    "README_*.md",
    "SUMMARY_*.txt",
    "SUMMARY_*.md",
    "REPORT_*.txt",
    "REPORT_*.md",
]

# 需要保留的文件（不移动）
KEEP_FILES = [
    "README.md",
    "SOCS_TODO.md",
    "dfx_runtime.txt",
    "AddressSegments.csv",
]

# 需要保留的目录
KEEP_DIRS = [
    "src",
    "tb",
    "script",
    "board_validation",
    "data",
    "doc",
    "hls",
    "logs",
    "archive",
    "versions",
    "validation",
    "scripts",
]

# =====================================================================
# 工具函数
# =====================================================================


def find_files_to_move(directory, patterns):
    """查找需要移动的文件"""
    files_to_move = []

    for pattern in patterns:
        for file_path in directory.glob(pattern):
            if file_path.is_file() and file_path.name not in KEEP_FILES:
                files_to_move.append(file_path)

    return files_to_move


def create_doc_structure(doc_dir):
    """创建文档目录结构"""
    # 创建子目录
    subdirs = [
        "project_status",
        "performance_reports",
        "verification_reports",
        "final_reports",
        "summaries",
    ]

    for subdir in subdirs:
        (doc_dir / subdir).mkdir(exist_ok=True)

    print(f"✓ 文档目录结构已创建: {doc_dir}")


def move_files(files, doc_dir):
    """移动文件到文档目录"""
    moved_count = 0

    for file_path in files:
        # 确定目标子目录
        filename = file_path.name.lower()

        if "project" in filename or "status" in filename:
            target_dir = doc_dir / "project_status"
        elif "performance" in filename or "correction" in filename:
            target_dir = doc_dir / "performance_reports"
        elif "verification" in filename:
            target_dir = doc_dir / "verification_reports"
        elif "final" in filename:
            target_dir = doc_dir / "final_reports"
        elif "summary" in filename or "report" in filename:
            target_dir = doc_dir / "summaries"
        else:
            target_dir = doc_dir / "other"

        # 确保目标目录存在
        target_dir.mkdir(exist_ok=True)

        # 移动文件
        target_path = target_dir / file_path.name

        # 避免覆盖
        if target_path.exists():
            # 添加数字后缀
            base = file_path.stem
            suffix = file_path.suffix
            counter = 1
            while target_path.exists():
                target_path = target_dir / f"{base}_{counter}{suffix}"
                counter += 1

        try:
            shutil.move(str(file_path), str(target_path))
            print(f"  ✓ 移动: {file_path.name} -> {target_path.relative_to(doc_dir)}")
            moved_count += 1
        except Exception as e:
            print(f"  ✗ 移动失败: {file_path.name} - {e}")

    return moved_count


def clean_empty_dirs(directory, keep_dirs):
    """清理空目录"""
    cleaned_count = 0

    for item in directory.iterdir():
        if item.is_dir() and item.name not in keep_dirs:
            try:
                # 检查目录是否为空
                if not any(item.iterdir()):
                    item.rmdir()
                    print(f"  ✓ 删除空目录: {item.name}")
                    cleaned_count += 1
            except Exception as e:
                print(f"  ✗ 删除目录失败: {item.name} - {e}")

    return cleaned_count


def create_index_file(doc_dir):
    """创建文档索引文件"""
    index_content = """# SOCS_HLS 文档索引

**最后更新**: 自动生成
**目录**: `source/SOCS_HLS/doc/`

## 目录结构

```
doc/
├── project_status/          # 项目状态文件
├── performance_reports/     # 性能报告
├── verification_reports/    # 验证报告
├── final_reports/          # 最终报告
├── summaries/              # 总结文件
└── other/                  # 其他文档
```

## 主要文档

### 项目状态
- `PROJECT_STATUS_FINAL.md` - 项目最终状态
- `PROJECT_COMPLETION_REPORT.md` - 项目完成报告

### 性能报告
- `PERFORMANCE_CORRECTION_SUMMARY.txt` - 性能修正总结
- `PERFORMANCE_METRICS_CORRECTION_COMPLETE.txt` - 性能指标修正完成

### 验证报告
- `VERIFICATION_REPORT.txt` - 验证报告
- `FINAL_VERIFICATION_REPORT.txt` - 最终验证报告

### 最终报告
- `FINAL_PROJECT_SUMMARY.md` - 最终项目总结
- `FINAL_PROJECT_SUMMARY.txt` - 最终项目总结文本

## 使用说明

1. **项目状态**: 查看项目当前进度和状态
2. **性能报告**: 了解性能指标修正历史
3. **验证报告**: 查看验证结果和测试数据
4. **最终报告**: 项目最终总结和结论

## 注意事项

- 这些文档是从 `source/SOCS_HLS/` 根目录移动而来
- 保持原始内容不变
- 定期更新索引文件
"""

    index_path = doc_dir / "INDEX.md"
    with open(index_path, "w", encoding="utf-8") as f:
        f.write(index_content)

    print(f"✓ 文档索引已创建: {index_path}")


# =====================================================================
# 主函数
# =====================================================================


def main():
    """主函数"""

    print("=" * 60)
    print("SOCS_HLS 目录整理工具")
    print("=" * 60)

    # 检查目录是否存在
    if not SOCS_HLS_DIR.exists():
        print(f"✗ 目录不存在: {SOCS_HLS_DIR}")
        return False

    # 创建文档目录
    print("\n1. 创建文档目录结构...")
    create_doc_structure(DOC_DIR)

    # 查找需要移动的文件
    print("\n2. 查找需要移动的文件...")
    files_to_move = find_files_to_move(SOCS_HLS_DIR, FILE_PATTERNS)

    if not files_to_move:
        print("  没有找到需要移动的文件")
        return True

    print(f"  找到 {len(files_to_move)} 个文件需要移动")

    # 移动文件
    print("\n3. 移动文件...")
    moved_count = move_files(files_to_move, DOC_DIR)

    # 清理空目录
    print("\n4. 清理空目录...")
    cleaned_count = clean_empty_dirs(SOCS_HLS_DIR, KEEP_DIRS)

    # 创建索引文件
    print("\n5. 创建文档索引...")
    create_index_file(DOC_DIR)

    # 打印总结
    print("\n" + "=" * 60)
    print("整理总结")
    print("=" * 60)
    print(f"移动文件数: {moved_count}")
    print(f"清理目录数: {cleaned_count}")
    print(f"文档目录: {DOC_DIR}")
    print("=" * 60)

    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
