#!/usr/bin/env python3
"""
生成多分辨率图像并转换为 BIN 格式
支持从源图像生成 256, 512, 1024, 2048 分辨率版本
"""

import os
import sys
from pathlib import Path
from PIL import Image
import numpy as np

# 添加 tool 目录到路径
sys.path.insert(0, str(Path(__file__).parent))
from png_bin_converter import write_bin_file


def generate_multi_resolution(source_image_path, output_dir, resolutions=[256, 512, 1024, 2048]):
    """
    从源图像生成多个分辨率版本并转换为 bin 格式
    
    Args:
        source_image_path: 源图像路径 (PNG)
        output_dir: 输出目录
        resolutions: 目标分辨率列表
    """
    # 读取源图像
    print(f"读取源图像: {source_image_path}")
    source_img = Image.open(source_image_path)
    source_arr = np.array(source_img, dtype=np.float32)
    
    print(f"源图像尺寸: {source_img.size}")
    print(f"源图像模式: {source_img.mode}")
    print(f"数据范围: [{source_arr.min():.2f}, {source_arr.max():.2f}]")
    
    # 创建输出目录
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # 生成各个分辨率版本
    for res in resolutions:
        print(f"\n{'='*60}")
        print(f"生成 {res}x{res} 分辨率版本")
        print(f"{'='*60}")
        
        # 缩放图像
        if res == source_img.size[0]:
            # 相同分辨率，直接使用原图
            resized_img = source_img
            resized_arr = source_arr
            print(f"使用原始图像（尺寸相同）")
        else:
            # 使用 LANCZOS 重采样（高质量）
            resized_img = source_img.resize((res, res), Image.Resampling.LANCZOS)
            resized_arr = np.array(resized_img, dtype=np.float32)
            print(f"缩放方法: LANCZOS")
        
        # 保存 PNG
        png_path = output_dir / f"{res}x{res}.png"
        resized_img.save(png_path)
        print(f"保存 PNG: {png_path}")
        
        # 转换为 BIN
        bin_path = output_dir / f"{res}x{res}.bin"
        write_bin_file(bin_path, resized_arr)
        
        # 验证
        print(f"验证数据范围: [{resized_arr.min():.2f}, {resized_arr.max():.2f}]")
        print(f"验证数据形状: {resized_arr.shape}")
    
    print(f"\n{'='*60}")
    print(f"完成！所有文件已保存到: {output_dir}")
    print(f"{'='*60}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="生成多分辨率图像并转换为 BIN 格式")
    parser.add_argument("source", help="源图像路径 (PNG)")
    parser.add_argument("-o", "--output", help="输出目录（默认为源图像所在目录）")
    parser.add_argument("-r", "--resolutions", nargs="+", type=int, 
                        default=[256, 512, 1024, 2048],
                        help="目标分辨率列表（默认: 256 512 1024 2048）")
    
    args = parser.parse_args()
    
    # 设置输出目录
    if args.output:
        output_dir = args.output
    else:
        output_dir = Path(args.source).parent
    
    # 生成多分辨率图像
    generate_multi_resolution(args.source, output_dir, args.resolutions)


if __name__ == "__main__":
    main()
