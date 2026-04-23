# SOCS Optimized HLS IP 板级验证指南

## 概述

本指南描述如何使用Vivado Hardware Manager和JTAG-to-AXI接口验证SOCS Optimized HLS IP在实际硬件上的功能。

**目标器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)  
**时钟频率**: 200MHz (5ns周期)  
**HLS IP**: calc_socs_optimized_hls (Vivado FFT IP版本)  
**验证日期**: 2026-04-23

## 验证流程

### 1. 环境准备

#### 1.1 硬件连接
- 确保FPGA板卡已上电
- JTAG调试器已连接到主机
- Vivado Hardware Manager可访问

#### 1.2 软件环境
- Vivado 2025.2 或更高版本
- Python 3.8+ (用于结果验证)
- 必要的Python包: numpy, matplotlib

#### 1.3 文件准备
确保以下文件存在：
```
e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized\
├── address_map.json                    # DDR地址映射配置
├── generate_socs_optimized_tcl.py      # TCL生成脚本
├── verify_socs_optimized_results.py    # 结果验证脚本
├── run_socs_optimized_validation.tcl   # 生成的TCL验证脚本
└── krn_r_combined.bin                  # 合并的kernel数据
└── krn_i_combined.bin                  # 合并的kernel数据
```

### 2. 生成验证脚本

如果尚未生成TCL脚本，运行：
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized
python generate_socs_optimized_tcl.py
```

**预期输出**:
```
============================================================
SOCS Optimized HLS IP Board Validation TCL Generator
============================================================

Combining kernel files...
  Loaded kernel 0: 81 elements
  Loaded kernel 1: 81 elements
  Loaded kernel 2: 81 elements
  Loaded kernel 3: 81 elements

Generated TCL script: e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized\run_socs_optimized_validation.tcl
Total size: 8331391 characters
```

### 3. Vivado Hardware Manager操作

#### 3.1 打开Hardware Manager
1. 启动Vivado
2. 打开Hardware Manager
3. 连接到硬件服务器: `localhost:3121`

#### 3.2 编程FPGA
```tcl
# 连接到硬件服务器
connect_hw_server -url localhost:3121
open_hw_target

# 编程FPGA
program_hw_devices [get_hw_devices]

# 重置JTAG-to-AXI Master
reset_hw_axi [get_hw_axis hw_axi_1]
```

#### 3.3 运行验证脚本
在Vivado Tcl Console中执行：
```tcl
# 加载生成的验证脚本
source e:/fpga-litho-accel/source/SOCS_HLS/board_validation/socs_optimized/run_socs_optimized_validation.tcl
```

**预期输出**:
```
=== SOCS Optimized HLS IP Board Validation ===
Connecting to hardware...
Programming FPGA...
Resetting JTAG-to-AXI...
Hardware ready.

Writing Mask Spectrum (Real) to DDR...
...
Mask Spectrum (Real) write complete.

Writing Mask Spectrum (Imag) to DDR...
...
Mask Spectrum (Imag) write complete.

Writing Eigenvalues to DDR...
...
Eigenvalues write complete.

Writing SOCS Kernels (Real) to DDR...
...
SOCS Kernels (Real) write complete.

Writing SOCS Kernels (Imag) to DDR...
...
SOCS Kernels (Imag) write complete.

Starting HLS IP execution...
HLS IP started. Waiting for completion...
HLS IP completed after X polls.
HLS IP execution complete.

Reading output from DDR...
...
output read complete.

=== Board Validation Complete ===
Results saved to: e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized
Run Python verification script to compare with Golden data.
```

### 4. 结果验证

#### 4.1 运行验证脚本
```bash
cd e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized
python verify_socs_optimized_results.py
```

**预期输出**:
```
============================================================
SOCS Optimized HLS IP Results Verification
============================================================

Expected output shape: (17, 17)
Expected elements: 289

Loading HLS output from TCL results...
Loaded HLS output: 289 elements

Loading golden output...
Loaded golden output: 262144 elements

============================================================
SOCS Optimized HLS IP Verification Results
============================================================

Output Shape: (17, 17)
Total Elements: 289

Error Metrics:
  RMSE: X.XXXXXXe-XX
  Max Absolute Difference: X.XXXXXXe-XX
  Mean Absolute Difference: X.XXXXXXe-XX
  Mean Relative Error: X.XXXXXXe-XX
  Correlation Coefficient: X.XXXXXX

✓ PASS - RMSE (X.XXXXXXe-XX) < tolerance (1e-03)

Saved comparison plot: e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized\socs_optimized_verification.png

Saved results to: e:\fpga-litho-accel\source\SOCS_HLS\board_validation\socs_optimized\verification_results.txt

============================================================
✓ VERIFICATION PASSED
============================================================
```

#### 4.2 查看结果文件
验证完成后，检查以下文件：
- `verification_results.txt` - 详细的验证结果
- `socs_optimized_verification.png` - 可视化比较图
- `output_chunk*.txt` - TCL读取的原始数据

### 5. 故障排除

#### 5.1 常见问题

**问题1**: TCL脚本执行失败
```
ERROR: [HLS 200-xxxx] ...
```
**解决方案**:
- 检查硬件连接
- 确认JTAG-to-AXI Master已重置
- 验证DDR地址映射是否正确

**问题2**: 验证失败，RMSE过高
```
✗ FAIL - RMSE (X.XXXXXXe-XX) >= tolerance (1e-03)
```
**解决方案**:
- 检查输入数据是否正确写入DDR
- 验证HLS IP是否正确执行
- 检查输出数据读取是否正确
- 考虑FFT IP仿真模型与RTL的差异

**问题3**: 输出数据全为零
**解决方案**:
- 检查HLS IP的ap_start信号
- 验证DDR地址映射
- 检查时钟和复位信号

#### 5.2 调试步骤

1. **验证输入数据**:
   ```tcl
   # 读取mskf_r前10个值验证
   create_hw_axi_txn read_debug [get_hw_axis hw_axi_1] \
       -type READ -address 0x40000000 -len 10
   run_hw_axi [get_hw_axi_txns read_debug]
   report_hw_axi_txn read_debug
   ```

2. **检查HLS IP状态**:
   ```tcl
   # 读取控制寄存器
   create_hw_axi_txn read_ctrl [get_hw_axis hw_axi_1] \
       -type READ -address 0x00000000 -len 1
   run_hw_axi [get_hw_axi_txns read_ctrl]
   report_hw_axi_txn read_ctrl
   ```

3. **验证输出地址**:
   ```tcl
   # 读取输出区域
   create_hw_axi_txn read_output [get_hw_axis hw_axi_1] \
       -type READ -address 0x44840000 -len 10
   run_hw_axi [get_hw_axi_txns read_output]
   report_hw_axi_txn read_output
   ```

### 6. 性能指标

#### 6.1 预期性能
- **时钟频率**: 200MHz (实际可达273.97MHz)
- **资源使用**:
  - DSP: 16 (vs 8,064 in direct DFT - 99.8% reduction)
  - BRAM: 39 (5%)
  - FF: 25,513 (7%)
  - LUT: 28,378 (17%)
- **处理时间**: 约1000-2000 cycles (5-10μs @ 200MHz)

#### 6.2 精度指标
- **目标RMSE**: < 1e-3 (0.1%)
- **预期RMSE**: 1e-5 to 1e-7 (基于C仿真结果)
- **FFT IP精度**: Float32精度，受仿真模型与RTL差异影响

### 7. 参考资料

#### 7.1 相关文档
- `AddressSegments.csv` - DDR地址映射
- `socs_config_optimized.h` - HLS IP配置
- `hls_config_optimized.cfg` - HLS综合配置
- `tb_socs_optimized.cpp` - HLS测试平台

#### 7.2 验证数据
- **Golden数据**: `e:\fpga-litho-accel\output\verification\`
- **Kernel数据**: `e:\fpga-litho-accel\output\verification\kernels\`
- **配置文件**: `e:\fpga-litho-accel\input\config\golden_original.json`

### 8. 更新日志

#### 2026-04-23
- 初始版本
- 支持Vivado FFT IP版本的SOCS HLS IP
- 集成DDR地址映射和JTAG-to-AXI验证
- 添加Python结果验证和可视化

---

**注意**: 本验证指南基于xcku3p-ffvb676-2-e器件和Vivado 2025.2环境。其他器件或工具版本可能需要调整。
