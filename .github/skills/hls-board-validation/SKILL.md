---
name: hls-board-validation
description: 'WORKFLOW SKILL — Execute board-level validation using JTAG-to-AXI for HLS IP hardware testing. USE FOR: connecting hardware; programming FPGA; starting HLS IP via ap_start; polling ap_done; reading output results. Essential for final hardware verification after synthesis. DO NOT USE FOR: simulation; synthesis; software testing.'
---

# HLS Board Validation Skill (JTAG-to-AXI)

## 适用场景

当用户需要执行以下任务时使用此skill:
- **硬件验证**: 对综合后的HLS IP进行实际板级测试
- **JTAG调试**: 通过JTAG-to-AXI接口控制HLS IP
- **实时测试**: 在实际FPGA硬件上验证功能和性能
- **结果读取**: 从硬件读取输出并与Golden数据对比

## 核心约束

**硬件环境**:
- Vivado Hardware Manager
- JTAG连接 (localhost:3121)
- 目标器件: `xcku3p-ffvb676-2-e`

**项目约束**:
- AXI-Lite基地址通常为 `0x40000000` (需根据综合报告确认)
- ap_done轮询offset通常为 `0x04`, bit[1]
- 输出buffer地址和长度根据具体IP确定
- **方案A（当前配置）**: 输出17×17 = 289 floats
- **方案B（目标配置）**: 输出65×65 = 4225 floats
- 必须与Golden数据对比验证正确性

## 执行流程 (Vivado TCL Console)

### 步骤 1: 连接硬件并编程

**TCL命令**:
```tcl
# 连接硬件服务器
connect_hw_server -url localhost:3121

# 打开硬件目标
open_hw_target

# 编程FPGA器件
program_hw_devices [get_hw_devices]
```

**验证内容**:
- JTAG连接成功
- FPGA正确识别和编程
- 无连接错误

**失败处理**:
- 检查JTAG链连接状态
- 验证bitstream文件正确
- 确认器件配置匹配

### 步骤 2: Reset JTAG-to-AXI Master

**TCL命令**:
```tcl
# Reset AXI Master控制器
reset_hw_axi [get_hw_axis hw_axi_1]
```

**目的**: 确保AXI Master处于初始状态,准备接收新事务

### 步骤 3: 启动HLS IP (写ap_start)

**TCL命令**:
```tcl
# 创建写事务: 启动IP执行
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 \
  -data 0x00000001 \
  -len 1 \
  -type write

# 执行写事务
run_hw_axi_txn start_txn
```

**关键参数**:
- **地址**: `0x40000000` - AXI-Lite Control接口基地址
- **数据**: `0x00000001` - 写ap_start寄存器(bit[0]=1)
- **长度**: 1 (单个32-bit写操作)

**注意**: 地址需根据实际综合报告调整

### 步骤 4: 等待ap_done (轮询+超时保护)

**推荐轮询方法**:
```tcl
# 定义轮询proc (建议封装)
proc wait_for_ap_done {axi_master base_addr timeout_ms} {
    set start_time [clock milliseconds]
    set done_offset 0x04
    set done_mask 0x2
    
    while {true} {
        # 读取状态寄存器
        create_hw_axi_txn status_txn $axi_master \
          -address [expr {$base_addr + $done_offset}] \
          -len 1 \
          -type read
        run_hw_axi_txn status_txn
        
        # 检查ap_done位
        set status [get_property DATA $status_txn]
        if {[expr {$status & $done_mask}] != 0} {
            return true
        }
        
        # 检查超时
        set elapsed [expr {[clock milliseconds] - $start_time}]
        if {$elapsed > $timeout_ms} {
            return false
        }
        
        # 等待1ms
        after 1
    }
}

# 使用轮询 (超时10秒)
set done [wait_for_ap_done [get_hw_axis hw_axi_1] 0x40000000 10000]
if {!$done} {
    puts "ERROR: ap_done timeout"
}
```

**关键参数**:
- **offset**: `0x04` (ap_done寄存器偏移)
- **mask**: `0x2` (bit[1]为ap_done标志)
- **timeout**: 建议10秒 (10000ms)

### 步骤 5: 读取输出结果

**TCL命令**:
```tcl
# 读取输出数据 (根据IP输出buffer调整)
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 \
  -len 128 \
  -type read

# 执行读事务
run_hw_axi_txn rd_txn

# 获取数据
set output_data [get_property DATA $rd_txn]
```

**关键参数**:
- **地址**: `0x40010000` - 输出buffer地址 (需根据IP确定)
- **长度**: 根据配置调整

**方案A（当前配置 Nx=4）** - 17×17 output:
```tcl
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 289 -type read
# 17×17 = 289 floats
```

**方案B（目标配置 Nx=16）** - 65×65 output:
```tcl
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 4225 -type read
# 65×65 = 4225 floats
```

**注意**: 当前推荐使用方案A进行快速验证

## 验证流程

### 步骤 6: 数据对比与验证

**对比内容**:
- 与Golden数据 (`output/verification/aerial_image_*.bin`)
- 与CPU reference (`litho.cpp` 输出)
- 确认Nx/Ny配置正确
- 验证IFFT改写结果

**验证标准**:
- 输出数据格式正确 (float, row-major)
- 与Golden数据误差在允许范围
- 峰值位置正确
- 数值范围合理

### 步骤 7: 性能测量 (可选)

**测量内容**:
- 实际执行时间 (从ap_start到ap_done)
- 与仿真Latency对比
- 实际时钟频率
- 功耗估算

## 错误处理

**常见问题**:

1. **JTAG连接失败**:
   - 检查物理连接
   - 验证JTAG服务器地址
   - 确认器件供电

2. **编程失败**:
   - 验证bitstream文件
   - 检查器件配置
   - 确认Flash编程状态

3. **ap_start无响应**:
   - 检查AXI-Lite地址映射
   - 验证时钟和复位
   - 确认IP正确集成

4. **ap_done超时**:
   - 检查IP设计逻辑
   - 验证时钟频率
   - 分析是否存在死锁

5. **输出数据错误**:
   - 检查输出buffer地址
   - 验证数据格式和长度
   - 对比Golden数据定位错误

**修复策略**:
- 提供具体TCL命令修复建议
- 分析可能的硬件问题
- 建议回到仿真阶段验证
- 提供调试命令和日志分析方法

## 完整TCL脚本模板

**建议保存为脚本** (`board_validation.tcl`):
```tcl
#!/usr/bin/tclsh

# FPGA-Litho HLS IP Board Validation Script
# Usage: 在Vivado Tcl Console中执行

# 1. 连接硬件
puts "Step 1: Connecting hardware..."
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
puts "Hardware connected and programmed"

# 2. Reset AXI Master
puts "Step 2: Resetting JTAG-to-AXI Master..."
reset_hw_axi [get_hw_axis hw_axi_1]

# 3. 启动IP
puts "Step 3: Starting HLS IP..."
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn
puts "IP started (ap_start written)"

# 4. 等待完成 (简化版轮询)
puts "Step 4: Waiting for ap_done..."
after 5000  # 等待5秒 (根据实际调整)

# 5. 读取结果
puts "Step 5: Reading output..."
create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] \
  -address 0x40010000 -len 128 -type read
run_hw_axi_txn rd_txn
set output_data [get_property DATA $rd_txn]
puts "Output read complete"

# 6. 数据处理建议
puts "Step 6: Please compare output_data with Golden files"
puts "Golden files located at: output/verification/aerial_image_*.bin"
```

## 下一步建议

完成板级验证后:
- **系统集成**: 将IP集成到更大系统
- **性能优化**: 根据实测结果进一步优化
- **自动化测试**: 开发自动化板级测试脚本
- **文档完善**: 记录验证结果和使用指南

## 参考文件

- **Golden数据**: `output/verification/aerial_image_tcc_direct.bin`
- **CPU Reference**: `verification/src/litho.cpp`
- **任务清单**: `source/SOCS_HLS/SOCS_TODO.md`
- **TCL模板**: `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl`