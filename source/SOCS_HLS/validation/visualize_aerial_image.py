#!/usr/bin/env python3
"""
空中像可视化脚本
用于可视化HLS输出和Golden数据的空中像

Author: FPGA-Litho Project
Version: 1.0
"""

import struct
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
import sys

# =====================================================================
# 配置
# =====================================================================

WORKSPACE_ROOT = Path("e:/fpga-litho-accel")
GOLDEN_DIR = WORKSPACE_ROOT / "output" / "verification"
HLS_OUTPUT_DIR = (
    WORKSPACE_ROOT / "source" / "SOCS_HLS" / "board_validation" / "socs_optimized"
)

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


def load_data(file_path):
    """加载数据文件"""
    if file_path.exists():
        data = read_bin_file(file_path)
        print(f"✓ 加载数据: {len(data)} 元素, 文件: {file_path}")
        return data
    else:
        print(f"✗ 文件未找到: {file_path}")
        return None


# =====================================================================
# 可视化函数
# =====================================================================


def create_2d_heatmap(
    data, output_shape=OUTPUT_SHAPE, title="空中像热力图", save_path=None
):
    """创建2D热力图"""

    # 重塑为2D
    data_2d = data.reshape(output_shape)

    # 创建图表
    fig, ax = plt.subplots(figsize=(8, 6))

    # 绘制热力图
    im = ax.imshow(data_2d, cmap="viridis", aspect="auto", interpolation="bilinear")

    # 添加颜色条
    cbar = plt.colorbar(im, ax=ax)
    cbar.set_label("强度")

    # 设置标题和标签
    ax.set_title(title, fontsize=14, fontweight="bold")
    ax.set_xlabel("X 坐标", fontsize=12)
    ax.set_ylabel("Y 坐标", fontsize=12)

    # 添加网格
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 2D热力图已保存: {save_path}")

    plt.close()


def create_3d_surface(
    data, output_shape=OUTPUT_SHAPE, title="空中像3D表面图", save_path=None
):
    """创建3D表面图"""

    # 重塑为2D
    data_2d = data.reshape(output_shape)

    # 创建网格
    x = np.arange(output_shape[1])
    y = np.arange(output_shape[0])
    X, Y = np.meshgrid(x, y)

    # 创建图表
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection="3d")

    # 绘制3D表面
    surf = ax.plot_surface(
        X, Y, data_2d, cmap=cm.viridis, linewidth=0, antialiased=True, alpha=0.8
    )

    # 添加颜色条
    cbar = fig.colorbar(surf, ax=ax, shrink=0.5, aspect=5)
    cbar.set_label("强度")

    # 设置标题和标签
    ax.set_title(title, fontsize=14, fontweight="bold")
    ax.set_xlabel("X 坐标", fontsize=12)
    ax.set_ylabel("Y 坐标", fontsize=12)
    ax.set_zlabel("强度", fontsize=12)

    # 设置视角
    ax.view_init(elev=30, azim=45)

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 3D表面图已保存: {save_path}")

    plt.close()


def create_contour_plot(
    data, output_shape=OUTPUT_SHAPE, title="空中像等高线图", save_path=None
):
    """创建等高线图"""

    # 重塑为2D
    data_2d = data.reshape(output_shape)

    # 创建网格
    x = np.arange(output_shape[1])
    y = np.arange(output_shape[0])
    X, Y = np.meshgrid(x, y)

    # 创建图表
    fig, ax = plt.subplots(figsize=(8, 6))

    # 绘制填充等高线
    contourf = ax.contourf(X, Y, data_2d, levels=20, cmap="viridis")

    # 绘制等高线
    contour = ax.contour(
        X, Y, data_2d, levels=20, colors="white", alpha=0.5, linewidths=0.5
    )

    # 添加颜色条
    cbar = plt.colorbar(contourf, ax=ax)
    cbar.set_label("强度")

    # 添加等高线标签
    ax.clabel(contour, inline=True, fontsize=8)

    # 设置标题和标签
    ax.set_title(title, fontsize=14, fontweight="bold")
    ax.set_xlabel("X 坐标", fontsize=12)
    ax.set_ylabel("Y 坐标", fontsize=12)

    # 添加网格
    ax.grid(True, alpha=0.3)

    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 等高线图已保存: {save_path}")

    plt.close()


def create_cross_sections(
    data, output_shape=OUTPUT_SHAPE, title="空中像截面图", save_path=None
):
    """创建截面图"""

    # 重塑为2D
    data_2d = data.reshape(output_shape)

    # 创建图表
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))

    # 中心行截面
    center_row = output_shape[0] // 2
    axes[0, 0].plot(data_2d[center_row, :], "b-", linewidth=2)
    axes[0, 0].set_title(
        f"中心行截面 (行 {center_row})", fontsize=12, fontweight="bold"
    )
    axes[0, 0].set_xlabel("列", fontsize=10)
    axes[0, 0].set_ylabel("强度", fontsize=10)
    axes[0, 0].grid(True, alpha=0.3)

    # 中心列截面
    center_col = output_shape[1] // 2
    axes[0, 1].plot(data_2d[:, center_col], "r-", linewidth=2)
    axes[0, 1].set_title(
        f"中心列截面 (列 {center_col})", fontsize=12, fontweight="bold"
    )
    axes[0, 1].set_xlabel("行", fontsize=10)
    axes[0, 1].set_ylabel("强度", fontsize=10)
    axes[0, 1].grid(True, alpha=0.3)

    # 对角线截面 (左上到右下)
    diagonal1 = np.diag(data_2d)
    axes[1, 0].plot(diagonal1, "g-", linewidth=2)
    axes[1, 0].set_title("对角线截面 (左上→右下)", fontsize=12, fontweight="bold")
    axes[1, 0].set_xlabel("位置", fontsize=10)
    axes[1, 0].set_ylabel("强度", fontsize=10)
    axes[1, 0].grid(True, alpha=0.3)

    # 对角线截面 (右上到左下)
    diagonal2 = np.diag(np.fliplr(data_2d))
    axes[1, 1].plot(diagonal2, "m-", linewidth=2)
    axes[1, 1].set_title("对角线截面 (右上→左下)", fontsize=12, fontweight="bold")
    axes[1, 1].set_xlabel("位置", fontsize=10)
    axes[1, 1].set_ylabel("强度", fontsize=10)
    axes[1, 1].grid(True, alpha=0.3)

    plt.suptitle(title, fontsize=14, fontweight="bold", y=1.02)
    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 截面图已保存: {save_path}")

    plt.close()


def create_comparison_dashboard(
    hls_data, golden_data, output_shape=OUTPUT_SHAPE, save_path=None
):
    """创建比较仪表板"""

    # 重塑为2D
    hls_2d = hls_data.reshape(output_shape)
    golden_2d = golden_data.reshape(output_shape)
    diff_2d = hls_2d - golden_2d

    # 创建图表
    fig = plt.figure(figsize=(16, 12))

    # 1. HLS输出热力图
    ax1 = fig.add_subplot(2, 3, 1)
    im1 = ax1.imshow(hls_2d, cmap="viridis", aspect="auto")
    ax1.set_title("HLS输出", fontsize=12, fontweight="bold")
    plt.colorbar(im1, ax=ax1)

    # 2. Golden输出热力图
    ax2 = fig.add_subplot(2, 3, 2)
    im2 = ax2.imshow(golden_2d, cmap="viridis", aspect="auto")
    ax2.set_title("Golden输出", fontsize=12, fontweight="bold")
    plt.colorbar(im2, ax=ax2)

    # 3. 差异热力图
    ax3 = fig.add_subplot(2, 3, 3)
    im3 = ax3.imshow(diff_2d, cmap="RdBu", aspect="auto")
    ax3.set_title("差异 (HLS - Golden)", fontsize=12, fontweight="bold")
    plt.colorbar(im3, ax=ax3)

    # 4. 中心行比较
    center_row = output_shape[0] // 2
    ax4 = fig.add_subplot(2, 3, 4)
    ax4.plot(hls_2d[center_row, :], "b-", label="HLS", linewidth=2)
    ax4.plot(golden_2d[center_row, :], "r--", label="Golden", linewidth=2)
    ax4.set_title(f"中心行比较 (行 {center_row})", fontsize=12, fontweight="bold")
    ax4.set_xlabel("列", fontsize=10)
    ax4.set_ylabel("强度", fontsize=10)
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    # 5. 中心列比较
    center_col = output_shape[1] // 2
    ax5 = fig.add_subplot(2, 3, 5)
    ax5.plot(hls_2d[:, center_col], "b-", label="HLS", linewidth=2)
    ax5.plot(golden_2d[:, center_col], "r--", label="Golden", linewidth=2)
    ax5.set_title(f"中心列比较 (列 {center_col})", fontsize=12, fontweight="bold")
    ax5.set_xlabel("行", fontsize=10)
    ax5.set_ylabel("强度", fontsize=10)
    ax5.legend()
    ax5.grid(True, alpha=0.3)

    # 6. 散点图
    ax6 = fig.add_subplot(2, 3, 6)
    ax6.scatter(golden_data, hls_data, alpha=0.5, s=10)
    ax6.plot(
        [golden_data.min(), golden_data.max()],
        [golden_data.min(), golden_data.max()],
        "r--",
        linewidth=2,
    )
    ax6.set_title("散点图: HLS vs Golden", fontsize=12, fontweight="bold")
    ax6.set_xlabel("Golden", fontsize=10)
    ax6.set_ylabel("HLS", fontsize=10)
    ax6.grid(True, alpha=0.3)

    plt.suptitle(
        "HLS输出与Golden数据比较仪表板", fontsize=14, fontweight="bold", y=1.02
    )
    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"✓ 比较仪表板已保存: {save_path}")

    plt.close()


# =====================================================================
# 主函数
# =====================================================================


def main():
    """主函数"""

    print("=" * 60)
    print("空中像可视化工具")
    print("=" * 60)

    # 检查命令行参数
    if len(sys.argv) > 1:
        data_file = Path(sys.argv[1])
        data_type = "custom"
    else:
        # 默认显示Golden数据
        data_file = GOLDEN_DIR / "aerial_image_socs_kernel.bin"
        data_type = "golden"

    # 加载数据
    print("\n1. 加载数据...")
    data = load_data(data_file)

    if data is None:
        print("✗ 数据加载失败")
        return False

    # 创建输出目录
    output_dir = HLS_OUTPUT_DIR / "visualization"
    output_dir.mkdir(exist_ok=True)

    # 创建可视化
    print("\n2. 创建可视化...")

    # 2D热力图
    heatmap_path = output_dir / "aerial_heatmap_2d.png"
    create_2d_heatmap(data, save_path=heatmap_path, title=f"空中像热力图 ({data_type})")

    # 3D表面图
    surface_path = output_dir / "aerial_surface_3d.png"
    create_3d_surface(
        data, save_path=surface_path, title=f"空中像3D表面图 ({data_type})"
    )

    # 等高线图
    contour_path = output_dir / "aerial_contour.png"
    create_contour_plot(
        data, save_path=contour_path, title=f"空中像等高线图 ({data_type})"
    )

    # 截面图
    cross_section_path = output_dir / "aerial_cross_sections.png"
    create_cross_sections(
        data, save_path=cross_section_path, title=f"空中像截面图 ({data_type})"
    )

    # 如果是Golden数据，创建比较仪表板
    if data_type == "golden":
        hls_file = HLS_OUTPUT_DIR / "output_aerial_image.bin"
        if hls_file.exists():
            print("\n3. 创建比较仪表板...")
            hls_data = load_data(hls_file)
            if hls_data is not None:
                dashboard_path = output_dir / "comparison_dashboard.png"
                create_comparison_dashboard(hls_data, data, save_path=dashboard_path)

    # 打印总结
    print("\n" + "=" * 60)
    print("可视化总结")
    print("=" * 60)
    print(f"数据类型: {data_type}")
    print(f"数据形状: {OUTPUT_SHAPE}")
    print(f"总元素数: {len(data)}")
    print(f"输出目录: {output_dir}")
    print("=" * 60)

    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
