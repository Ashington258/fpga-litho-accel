# JTAG-to-AXI 板级验证指南

本目录用于通过 **JTAG-to-AXI Master** 方式加载验证数据到 DDR，执行 SOCS HLS IP 硬件验证。

## 📁 文件说明

| 文件                       | 用途                                        |
| -------------------------- | ------------------------------------------- |
| `socs_hls_validation.tcl`  | 主验证脚本，包含 AXI 读写函数和 HLS IP 控制 |
| `bin_to_tcl_converter.py`  | Python 工具，将 BIN 文件转换为 TCL hex 格式 |
| `data/socs_data.tcl`       | 小型验证数据（scales、kernels、参考输出）   |
| `data/socs_data_batch.tcl` | 大型分批数据（mskf_r/i，512×512 floats）    |
| `data/data_usage.tcl`      | 数据写入地址和使用说明                      |

## 🔧 硬件要求

- **FPGA 开发板**：已烧录 SOCS HLS IP 的 Bitstream
- **Vivado Hardware Manager**：已连接 JTAG
- **JTAG-to-AXI Master IP**：已配置在 Block Design 中

## 📊 验证数据概览

| 数据名称     | 大小               | DDR 地址   | 写入方式            |
| ------------ | ------------------ | ---------- | ------------------- |
| `mskf_r`     | 512×512 (1MB)      | 0x40000000 | 分批 (2049 batches) |
| `mskf_i`     | 512×512 (1MB)      | 0x42000000 | 分批 (2049 batches) |
| `scales`     | 10 floats          | 0x44000000 | 直接写入            |
| `krn_r`      | 10×81 floats       | 0x44400000 | 直接写入            |
| `krn_i`      | 10×81 floats       | 0x44800000 | 直接写入            |
| **HLS 输出** | 17×17 (289 floats) | 0x44840000 | 读取                |

## 🚀 使用步骤

### Step 1: 连接硬件并加载脚本

在 Vivado Tcl Console 中执行：

```tcl
# 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 切换到验证目录
cd e:/fpga-litho-accel/validation/board/jtag

# 加载主验证脚本（包含 axi_write_data 和 axi_read_data 函数）
source socs_hls_validation.tcl

# 获取 JTAG-to-AXI Master 接口
set axi_if [get_hw_axis hw_axi_1]
```

### Step 2: 加载小型数据（直接写入）

```tcl
# 加载验证数据变量定义
source data/socs_data.tcl

# 写入 scales (10 floats @ 0x44000000)
axi_write_data $axi_if 0x44000000 $scales_data "Write scales"

# 写入 kernels (10个kernel，各81 floats)
for {set k 0} {$k < 10} {incr k} {
    # 注意：使用 [set krn_${k}_r_data] 获取变量值
    axi_write_data $axi_if [expr 0x44400000 + $k*81*4] [set krn_${k}_r_data] "Write krn_$k_r"
    axi_write_data $axi_if [expr 0x44800000 + $k*81*4] [set krn_${k}_i_data] "Write krn_$k_i"
}
```

### Step 3: 加载大型数据（分批写入）

⚠️ **注意**：mskf_r/i 数据量大（各1MB），分批写入耗时约 **30-40分钟**

```tcl
# 加载分批数据（文件较大，约5MB）
source data/socs_data_batch.tcl

# 分批写入 mskf_r (2049 batches, 每批 128 floats)
set num_batches 2049
puts "开始写入 mskf_r..."
for {set b 0} {$b < $num_batches} {incr b} {
    set batch_addr [expr 0x40000000 + $b*128*4]
    axi_write_data $axi_if $batch_addr [set mskf_r_batch_$b] "mskf_r batch $b"
    
    # 每100批次打印进度
    if {$b % 100 == 0} {
        puts "进度: mskf_r batch $b / $num_batches"
    }
}

# 分批写入 mskf_i (2049 batches)
puts "开始写入 mskf_i..."
for {set b 0} {$b < $num_batches} {incr b} {
    set batch_addr [expr 0x42000000 + $b*128*4]
    axi_write_data $axi_if $batch_addr [set mskf_i_batch_$b] "mskf_i batch $b"
    
    if {$b % 100 == 0} {
        puts "进度: mskf_i batch $b / $num_batches"
    }
}

puts "数据加载完成！"
```

### Step 4: 启动 HLS IP 执行

```tcl
# 写入 ap_start = 1 启动 HLS IP
create_hw_axi_txn start_txn $axi_if -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn
puts "HLS IP 已启动"

# 等待 ap_done（轮询状态寄存器）
wait_for_ap_done $axi_if
puts "HLS IP 执行完成"
```

### Step 5: 读取 HLS 输出结果

```tcl
# 读取输出数据 (289 floats @ 0x44840000)
# 由于 Vivado burst 限制，分3批读取：128+128+33
axi_read_data $axi_if 0x44840000 128 "Read output batch 1"
axi_read_data $axi_if 0x44840200 128 "Read output batch 2"
axi_read_data $axi_if 0x44840400 33  "Read output batch 3"

puts "输出数据已读取到 TCL 变量"
```

### Step 6: 验证输出数据

```tcl
# 比较输出与参考数据 tmpImgp_pad32_data
# 注意：HLS 输出为 17×17=289 floats，参考数据为 32×32=1024 floats
# 需要提取 17×17 区域进行比较

# 显示前10个输出值
puts "HLS 输出前10个值: [lrange $rd_data_0 0 9]"
puts "参考数据前10个值: [lrange $tmpImgp_pad32_data 0 9]"
```

## ⚡ 快速测试流程（使用内置示例数据）

如果只想快速测试硬件连通性，可以使用脚本内置的示例数据：

```tcl
# 加载主脚本
source socs_hls_validation.tcl

# 执行完整验证流程（使用示例数据）
run_socs_validation
```

此命令会：
1. 写入示例测试数据到 DDR
2. 启动 HLS IP
3. 等待执行完成
4. 读取输出并显示

## 🔍 常见问题排查

### 1. 地址格式错误

```
[Labtools 27-3199] Address Value '1149501440' is too large
```

**解决方案**：脚本已修复，使用 hex 格式地址 `0x44840000`

### 2. Burst 长度超限

```
ERROR: Burst length exceeds maximum supported
```

**解决方案**：MAX_BURST_LEN=128，大数据需分批写入

### 3. ap_done 未检测到

- 检查 HLS IP 是否正确配置
- 确认 Bitstream 包含 HLS IP
- 查看 ILA 波形分析

## 📝 数据重新生成

如果需要从最新的验证数据生成 TCL 文件：

```bash
cd e:\fpga-litho-accel\validation\board\jtag
python bin_to_tcl_converter.py
```

输入数据来源：`output/verification_original/`

## 📈 性能参考

| 操作            | 数据量        | 耗时（参考） |
| --------------- | ------------- | ------------ |
| 写入 scales     | 10 floats     | < 1秒        |
| 写入 10 kernels | 1620 floats   | ~5秒         |
| 写入 mskf_r     | 262144 floats | ~15分钟      |
| 写入 mskf_i     | 262144 floats | ~15分钟      |
| HLS IP 执行     | -             | < 1秒        |
| 读取输出        | 289 floats    | < 1秒        |

**总计**：完整验证流程约 **30-40分钟**（主要耗时在数据加载）

## 🔗 相关文档

- [公共 AXI 测试脚本](../common/axi_memory_test.tcl)
- [板级验证主指南](../README.md)
- [HLS 验证流程报告](../../source/SOCS_HLS/doc/HLS验证流程完整报告.md)