# 🔴 JTAG-to-AXI 超时问题修复指南

## 问题诊断

### 现象
- 所有 JTAG-to-AXI 读取操作超时：`ERROR: [Xicom 50-38] xicom: AXI TRANSACTION TIMED OUT`
- 地址 0x00000000 (HLS Control)、0x40000000 (BRAM) 等均无法访问

### 根本原因

**复位配置错误**：

```tcl
# 当前错误配置（从 Vivado 日志）
set_property CONFIG.CONST_VAL {0} [get_bd_cells xlconstant_1]
connect_bd_net [get_bd_pins xlconstant_1/dout] [get_bd_pins proc_sys_reset_0/ext_reset_in]
```

**问题分析**：
1. `xlconstant_1/dout` = **0** 连接到 `proc_sys_reset_0/ext_reset_in`
2. 对于 `proc_sys_reset` IP，**ext_reset_in = 0 表示处于复位状态**
3. `peripheral_aresetn` 输出保持为 **0（复位态）**
4. 所有 AXI 设备被强制复位 → AXI 总线无响应 → JTAG 读取超时

---

## ✅ 修复方案

### 方案 1：使用板卡复位信号（推荐生产环境）

**步骤**：
1. 打开 Vivado Block Design
2. 删除 `xlconstant_1` IP
3. 创建外部复位端口：
   ```tcl
   create_bd_port -dir I -type rst reset
   set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports reset]
   ```
4. 连接复位端口：
   ```tcl
   connect_bd_net [get_bd_ports reset] [get_bd_pins proc_sys_reset_0/ext_reset_in]
   ```
5. 重新生成、综合、实现、编程 FPGA

**TCL 自动化脚本**：

```tcl
# 文件: fix_reset_board.tcl
# 执行: source fix_reset_board.tcl

open_bd_design {E:/1.Project/4.FPGA/vivado/fpga-litho-accel/fpga-litho-accel.srcs/sources_1/bd/design_1/design_1.bd}

puts "步骤 1: 删除错误的常量 IP..."
delete_bd_objs [get_bd_cells xlconstant_1]

puts "步骤 2: 创建板卡复位端口..."
create_bd_port -dir I -type rst reset
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports reset]

puts "步骤 3: 连接复位信号..."
connect_bd_net [get_bd_ports reset] [get_bd_pins proc_sys_reset_0/ext_reset_in]

puts "步骤 4: 验证设计..."
validate_bd_design

puts "步骤 5: 保存设计..."
save_bd_design
regenerate_bd_layout

puts "步骤 6: 重新生成 HDL..."
generate_target all [get_files E:/1.Project/4.FPGA/vivado/fpga-litho-accel/fpga-litho-accel.srcs/sources_1/bd/design_1/design_1.bd]

puts "修复完成！请执行以下步骤："
puts "  1. reset_run synth_1"
puts "  2. launch_runs synth_1 -jobs 14"
puts "  3. launch_runs impl_1 -to_step write_bitstream -jobs 14"
puts "  4. 重新编程 FPGA"
```

---

### 方案 2：常量解除复位（临时测试用）

**适用于**：无外部复位按钮，或快速验证 AXI 连接

```tcl
# 文件: fix_reset_constant.tcl
# 执行: source fix_reset_constant.tcl

open_bd_design {E:/1.Project/4.FPGA/vivado/fpga-litho-accel/fpga-litho-accel.srcs/sources_1/bd/design_1/design_1.bd}

puts "修改 xlconstant_1 值为 1（解除复位）..."
set_property CONFIG.CONST_VAL {1} [get_bd_cells xlconstant_1]

validate_bd_design
save_bd_design
regenerate_bd_layout

puts "修复完成！重新综合和实现..."
```

---

## 验证修复

修复后重新编程 FPGA，然后运行以下 TCL 测试：

```tcl
# 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
current_hw_device [get_hw_devices xcku3p_0]

# 测试 HLS 控制接口
set axi_master [lindex [get_hw_axis] 0]
reset_hw_axi $axi_master

create_hw_axi_txn test_hls $axi_master -type read -address 00000000 -len 4 -force
run_hw_axi [get_hw_axi_txns test_hls]
report_hw_axi_txn [get_hw_axi_txns test_hls]

# 预期结果：
# - 地址 0x00000000: 读取成功，返回 HLS Control 寄存器值（ap_start, ap_done 等）
# - 地址 0x40000000: 读取成功，返回 BRAM 内容
```

---

## 地址映射参考

修复后，以下地址应该可正常访问：

| 地址范围 | 设备 | 用途 |
|---------|------|------|
| 0x00000000 - 0x0000FFFF | HLS Control (64K) | ap_start, ap_done, ap_idle 寄存器 |
| 0x00010000 - 0x0001FFFF | HLS Data (64K) | 输入/输出数据缓冲 |
| 0x40000000 - 0x4003FFFF | BRAM Controller 0 (256K) | mask 频域数据 |
| 0x40100000 - 0x4013FFFF | BRAM Controller 1 (256K) | scales/kernel 数据 |
| 0x40200000 - 0x40207FFF | BRAM Controller 2 (32K) | 输出结果 |

---

## 常见问题排查

### Q1: 修复后仍然超时？
检查：
- `clk_wiz_0/locked` 是否为 1（时钟锁定）
- `proc_sys_reset_0/peripheral_aresetn` 是否为 1（解除复位）
- SmartConnect 是否正确连接到所有 Master/Slave

### Q2: HLS IP 不启动？
检查：
- 写入 `ap_start` (地址 0x00) 的值为 1
- 检查 `ap_done` (地址 0x04, bit 1) 是否置位
- 确保 HLS IP 的时钟和复位信号正确

---

## 总结

**问题**：`xlconstant_1` = 0 导致整个系统处于复位状态
**修复**：连接有效的复位信号或设置常量为 1
**验证**：JTAG-to-AXI 读取成功，AXI 总线响应正常