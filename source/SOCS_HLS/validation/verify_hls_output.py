#!/usr/bin/env python3
"""
HLS输出与Golden数据验证脚本
用于验证HLS IP输出结果与Golden数据的一致性

Author: FPGA-Litho Project
Version: 1.0
"""

import struct
import json
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import sys

# =====================================================================
# 配置
# =====================================================================

WORKSPACE_ROOT = Path("e:/fpga-litho-accel")
GOLDEN_DIR = WORKSPACE_ROOT / "output" / "verification"
HLS_OUTPUT_DIR = (
    WORKSPACE_ROOT / "source" / "SOCS_HLS" / "board_validation" / "socs_optimized"
)
CONFIG_FILE = HLS_OUTPUT_DIR / "address_map.json"

# 输出形状配置
OUTPUT_SHAPE = (17, 17)  # 17x17 = 289 floats

# =====================================================================
# 数据加载函数
# =====================================================================


def read_bin_file(filepath):
    """读取BIN文件为float32数组"""
    with open(filepath, "rb") as f:
        data = f.read()

    # 解包为float32
    num_floats = len(data) // 4
    floats = struct.unpack(f"{num_floats}f", data)

    return np.array(floats, dtype=np.float32)


def load_golden_output():
    """加载Golden输出数据"""
    golden_file = GOLDEN_DIR / "aerial_image_socs_kernel.bin"
    if golden_file.exists():
        golden_data = read_bin_file(golden_file)
        print(f"✓ 加载Golden数据: {len(golden_data)} 元素")
        return golden_data
    else:
        print(f"✗ Golden文件未找到: {golden_file}")
        return None


def load_hls_output(output_file):
    """加载HLS输出数据"""
    if output_file.exists():
        hls_data = read_bin_file(output_file)
        print(f"✓ 加载HLS输出: {len(hls_data)} 元素")
        return hls_data
    else:
        print(f"✗ HLS输出文件未找到: {output_file}")
        return None


# =====================================================================
# 验证函数
# =====================================================================


def calculate_rmse(predicted, target):
    """计算均方根误差"""
    if len(predicted) != len(target):
        raise ValueError(f"数组长度不匹配: {len(predicted)} vs {len(target)}")

    diff = predicted - target
    mse = np.mean(diff**2)
    rmse = np.sqrt(mse)

    return rmse


def calculate_relative_error(predicted, target):
    """计算相对误差"""
    if len(predicted) != len(target):
        raise ValueError(f"数组长度不匹配: {len(predicted)} vs {len(target)}")

    # 避免除零
    mask = np.abs(target) > 1e-10
    if np.sum(mask) == 0:
        return np.inf

    relative_error = np.abs(predicted[mask] - target[mask]) / np.abs(target[mask])
    mean_relative_error = np.mean(relative_error)

    return mean_relative_error


def verify_results(hls_output, golden_output, output_shape=OUTPUT_SHAPE):
    """验证HLS输出与Golden数据"""

    print("\n" + "=" * 60)
    print("HLS输出与Golden数据验证")
    print("=" * 60)

    # 重塑为2D用于可视化
    hls_2d = hls_output.reshape(output_shape)
    golden_2d = golden_output.reshape(output_shape)

    # 计算指标
    rmse = calculate_rmse(hls_output, golden_output)
    max_diff = np.max(np.abs(hls_output - golden_output))
    mean_diff = np.mean(np.abs(hls_output - golden_output))
    relative_error = calculate_relative_error(hls_output, golden_output)

    # 计算相关系数
    correlation = np.corrcoef(hls_output, golden_output)[0, 1]

    # 打印结果
    print(f"\n输出形状: {output_shape}")
    print(f"总元素数: {len(hls_output)}")

    print(f"\n误差指标:")
    print(f"  RMSE: {rmse:.6e}")
    print(f"  最大绝对误差: {max_diff:.6e}")
    print(f"  平均绝对误差: {mean_diff:.6e}")
    print(f"  平均相对误差: {relative_error:.6e}")
    print(f"  相关系数: {correlation:.6f}")

    # 检查是否在容差范围内
    tolerance = 1e-3  # 0.1% 容差
    if rmse < tolerance:
        print(f"\n✓ 通过 - RMSE ({rmse:.6e}) < 容差 ({tolerance})")
        return True, rmse, correlation
    else:
        print(f"\n✗ 失败 - RMSE ({rmse:.6e}) >= 容差 ({tolerance})")
        return False, rmse, correlation


# =====================================================================
# 可视化函数
# =====================================================================


def create_comparison_plots(
    hls_output, golden_output, output_shape=OUTPUT_SHAPE, save_path=None
):
    """创建比较图表"""

    # 重塑为2D
    hls_2d = hls_output.reshape(output_shape)
    golden_2d = golden_output.reshape(output_shape)
    diff_2d = hls_2d - golden_2d

    # 创建图表
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))

    # 绘制HLS输出
    im1 = axes[0, 0].imshow(hls_2d, cmap="viridis", aspect="auto")
    axes[0, 0].set_title("HLS输出")
    plt.colorbar(im1, ax=axes[0, 0])

    # 绘制Golden输出
    im2 = axes[0, 1].imshow(golden_2d, cmap="viridis", aspect="auto")
    axes[0, 1].set_title("Golden输出")
    plt.colorbar(im2, ax=axes[0, 1])

    # 绘制差异
    im3 = axes[0, 2].imshow(diff_2d, cmap="RdBu", aspect="auto")
    axes[0, 2].set_title("差异 (HLS - Golden)")
    plt.colorbar(im3, ax=axes[0, 2])

    # 绘制中心行比较
    center_row = output_shape[0] // 2
    axes[1, 0].plot(hls_2d[center_row, :], "b-", label="HLS", linewidth=2)
    axes[1, 0].plot(golden_2d[center_row, :], "r--", label="Golden", linewidth=2)
    axes[1, 0].set_title(f"中心行比较 (行 {center_row})")
    axes[1, 0].set_xlabel("列")
    axes[1, 0].set_ylabel("强度")
    axes[1, 0].legend()
    axes[1, 0].grid(True)

    # 绘制中心列比较
    center_col = output_shape[1] // 2
    axes[1, 1].plot(hls_2d[:, center_col], "b-", label="HLS", linewidth=2)
    axes[1, 1].plot(golden_2d[:, center_col], "r--", label="Golden", linewidth=2)
    axes[1, 1].set_title(f"中心列比较 (列 {center_col})")
    axes[1, 1].set_xlabel("行")
    axes[1, 1].set_ylabel("强度")
    axes[1, 1].legend()
    axes[1, 1].grid(True)

    # 绘制散点图
    axes[1, 2].scatter(golden_output, hls_output, alpha=0.5, s=10)
    axes[1, 2].plot(
        [golden_output.min(), golden_output.max()],
        [golden_output.min(), golden_output.max()],
        "r--",
        linewidth=2,
    )
    axes[1, 2].set_title("散点图: HLS vs Golden")
    axes[1, 2].set_xlabel("Golden")
    axes[1, 2].set_ylabel("HLS")
    axes[1, 2].grid(True)

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 图表已保存: {save_path}")

    plt.close()


def create_aerial_visualization(
    aerial_data, output_shape=OUTPUT_SHAPE, save_path=None, title="空中像"
):
    """创建空中像可视化"""

    # 重塑为2D
    aerial_2d = aerial_data.reshape(output_shape)

    # 创建图表
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    # 绘制空中像
    im1 = axes[0].imshow(aerial_2d, cmap="viridis", aspect="auto")
    axes[0].set_title(title)
    plt.colorbar(im1, ax=axes[0])

    # 绘制3D表面
    x = np.arange(output_shape[1])
    y = np.arange(output_shape[0])
    X, Y = np.meshgrid(x, y)

    ax3d = fig.add_subplot(132, projection="3d")
    surf = ax3d.plot_surface(
        X, Y, aerial_2d, cmap="viridis", linewidth=0, antialiased=True
    )
    ax3d.set_title("3D表面图")
    ax3d.set_xlabel("X")
    ax3d.set_ylabel("Y")
    ax3d.set_zlabel("强度")

    # 绘制等高线
    im3 = axes[2].contourf(X, Y, aerial_2d, levels=20, cmap="viridis")
    axes[2].set_title("等高线图")
    axes[2].set_xlabel("X")
    axes[2].set_ylabel("Y")
    plt.colorbar(im3, ax=axes[2])

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 空中像可视化已保存: {save_path}")

    plt.close()


# =====================================================================
# 主函数
# =====================================================================


def main():
    """主函数"""

    print("=" * 60)
    print("HLS输出与Golden数据验证工具")
    print("=" * 60)

    # 检查命令行参数
    if len(sys.argv) > 1:
        hls_output_file = Path(sys.argv[1])
    else:
        # 默认HLS输出文件
        hls_output_file = HLS_OUTPUT_DIR / "output_aerial_image.bin"

    # 加载数据
    print("\n1. 加载数据...")
    golden_data = load_golden_output()
    hls_data = load_hls_output(hls_output_file)

    if golden_data is None or hls_data is None:
        print("✗ 数据加载失败")
        return False

    # 验证数据长度
    if len(golden_data) != len(hls_data):
        print(f"✗ 数据长度不匹配: Golden={len(golden_data)}, HLS={len(hls_data)}")
        return False

    # 验证结果
    print("\n2. 验证结果...")
    passed, rmse, correlation = verify_results(hls_data, golden_data)

    # 创建比较图表
    print("\n3. 创建比较图表...")
    comparison_plot_path = HLS_OUTPUT_DIR / "hls_golden_comparison.png"
    create_comparison_plots(hls_data, golden_data, save_path=comparison_plot_path)

    # 创建空中像可视化
    print("\n4. 创建空中像可视化...")
    aerial_plot_path = HLS_OUTPUT_DIR / "aerial_image_visualization.png"
    create_aerial_visualization(
        hls_data, save_path=aerial_plot_path, title="HLS输出空中像"
    )

    golden_aerial_plot_path = HLS_OUTPUT_DIR / "golden_aerial_visualization.png"
    create_aerial_visualization(
        golden_data, save_path=golden_aerial_plot_path, title="Golden空中像"
    )

    # 保存验证报告
    print("\n5. 保存验证报告...")
    report = {
        "timestamp": str(np.datetime64("now")),
        "hls_output_file": str(hls_output_file),
        "golden_file": str(GOLDEN_DIR / "aerial_image_socs_kernel.bin"),
        "output_shape": OUTPUT_SHAPE,
        "total_elements": len(hls_data),
        "rmse": float(rmse),
        "correlation": float(correlation),
        "passed": passed,
        "tolerance": 1e-3,
    }

    report_path = HLS_OUTPUT_DIR / "verification_report.json"
    with open(report_path, "w") as f:
        json.dump(report, f, indent=2)

    print(f"✓ 验证报告已保存: {report_path}")

    # 打印总结
    print("\n" + "=" * 60)
    print("验证总结")
    print("=" * 60)
    print(f"状态: {'✓ 通过' if passed else '✗ 失败'}")
    print(f"RMSE: {rmse:.6e}")
    print(f"相关系数: {correlation:.6f}")
    print(f"容差: {1e-3:.6e}")
    print("=" * 60)

    return passed


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
