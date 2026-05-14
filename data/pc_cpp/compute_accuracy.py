#!/usr/bin/env python3
"""
计算 SOCS 成像精度指标
比较 SOCS 输出与 TCC 直接成像（Golden Reference）
"""

import numpy as np
import json
import os

def read_binary_float32(filepath, count):
    """读取二进制 float32 文件"""
    with open(filepath, 'rb') as f:
        data = np.fromfile(f, dtype=np.float32, count=count)
    return data

def compute_metrics(socs_image, tcc_image):
    """计算精度指标"""
    # 确保数组大小一致
    assert len(socs_image) == len(tcc_image), f"Size mismatch: {len(socs_image)} vs {len(tcc_image)}"
    
    # 绝对误差
    abs_error = np.abs(socs_image - tcc_image)
    
    # 相对误差（避免除零）
    epsilon = 1e-10
    rel_error = abs_error / (np.abs(tcc_image) + epsilon)
    
    # 计算各项指标
    metrics = {
        "max_absolute_error": float(np.max(abs_error)),
        "min_absolute_error": float(np.min(abs_error)),
        "mean_absolute_error": float(np.mean(abs_error)),
        "max_relative_error": float(np.max(rel_error)),
        "min_relative_error": float(np.min(rel_error)),
        "mean_relative_error": float(np.mean(rel_error)),
        "rmse": float(np.sqrt(np.mean(abs_error ** 2))),
        "num_pixels": len(socs_image)
    }
    
    return metrics

def main():
    base_dir = "data/pc_cpp"
    image_size = 1024 * 1024  # 1024×1024
    
    # 读取 TCC 直接成像作为 Golden Reference
    tcc_image_file = os.path.join(base_dir, "output_10kernels", "aerial_image_tcc_direct.bin")
    tcc_image = read_binary_float32(tcc_image_file, image_size)
    
    print("=" * 60)
    print("SOCS 成像精度评估")
    print("=" * 60)
    print(f"Golden Reference: {tcc_image_file}")
    print(f"Image Size: 1024×1024 = {image_size} pixels")
    print()
    
    results = {}
    
    # 评估不同核数量的精度
    for num_kernels in [10, 50, 400]:
        output_dir = os.path.join(base_dir, f"output_{num_kernels}kernels")
        socs_image_file = os.path.join(output_dir, "aerial_image_socs_kernel.bin")
        
        if not os.path.exists(socs_image_file):
            print(f"⚠️  {num_kernels} kernels: File not found: {socs_image_file}")
            continue
        
        # 读取 SOCS 成像结果
        socs_image = read_binary_float32(socs_image_file, image_size)
        
        # 计算精度指标
        metrics = compute_metrics(socs_image, tcc_image)
        
        results[f"{num_kernels}_kernels"] = metrics
        
        print(f"📊 {num_kernels} Kernels:")
        print(f"  Max Absolute Error: {metrics['max_absolute_error']:.6e}")
        print(f"  Max Relative Error: {metrics['max_relative_error']:.6e}")
        print(f"  RMSE:               {metrics['rmse']:.6e}")
        print(f"  Mean Absolute Error: {metrics['mean_absolute_error']:.6e}")
        print()
    
    # 保存结果
    output_file = os.path.join(base_dir, "accuracy_metrics.json")
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    
    print("=" * 60)
    print(f"✅ Results saved to: {output_file}")
    print("=" * 60)

if __name__ == "__main__":
    main()
