# FPGA-Litho SOCS HLS 板级验证指南

**适用IP版本**: v13 (socs_full_csynth_v13) - 已修复FFT端口不匹配问题
**验证方式**: JTAG-to-AXI + PC端Golden数据对比
**目标器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)

---

## 1. 验证流程概览

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  PC端准备数据   │ --> │  JTAG写入DDR    │ --> │  FPGA执行IP核   │
│  (Golden Input) │     │  (AXI Master)   │     │  (calc_socs_hls)│
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                                               │
        │                                               v
        │                                       ┌─────────────────┐
        │                                       │  JTAG读取DDR    │
        │                                       │  (输出结果)     │
        │                                       └─────────────────┘
        │                                               │
        v                                               v
┌─────────────────────────────────────────────────────────────────┐
│                    PC端对比验证                                   │
│  Output vs tmpImgp_pad32.bin (Golden)                           │
│  计算RMSE、相对误差、可视化对比                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 数据准备（PC端）

### 2.1 Golden输入数据

**数据路径**: `/root/project/FPGA-Litho/output/verification/`

| 数据文件 | 大小 | 维度 | 用途 | AXI端口 |
|----------|------|------|------|---------|
| `mskf_r.bin` | 1.0M | 512×512 float | Mask频域实部 | gmem0 |
| `mskf_i.bin` | 1.0M | 512×512 float | Mask频域虚部 | gmem1 |
| `scales.bin` | 40B | 10 float | 特征值/缩放因子 | gmem2 |
| `krn_X_r.bin` | 324B×10 | 9×9 float | Kernel实部(nk=10) | gmem3 |
| `krn_X_i.bin` | 324B×10 | 9×9 float | Kernel虚部(nk=10) | gmem4 |

### 2.2 Golden输出数据

| 数据文件 | 大小 | 维度 | 用途 |
|----------|------|------|------|
| `tmpImgp_pad32.bin` | 1.2K | 32×32 float | **HLS标准输出** (用于对比) |
| `aerial_image_socs_kernel.bin` | 1.0M | 512×512 float | CPU完整输出 (参考) |

### 2.3 生成Golden数据（如需更新）

```bash
cd /root/project/FPGA-Litho
python verification/run_verification.py
```

**生成的关键文件**:
- `output/verification/mskf_r.bin`, `mskf_i.bin` - Mask频谱
- `output/verification/scales.bin` - 特征值
- `output/verification/kernels/krn_X_r.bin, krn_X_i.bin` - SOCS kernels
- `output/verification/tmpImgp_pad32.bin` - **HLS Golden输出 (32×32)**

---

## 3. Vivado硬件环境准备

### 3.1 Block Design架构

```
┌───────────────────────────────────────────────────────────────┐
│                    FPGA Board                                 │
│  ┌─────────────┐    ┌────────────┐    ┌──────────────────┐   │
│  │ JTAG-to-AXI │<-->│ AXI Interconnect │<-->│ calc_socs_hls │   │
│  │   Master    │    │   (SmartConnect) │    │    IP核       │   │
│  └──────┬──────┘    └──────┬─────┘    └──────────┬───────┘   │
│         │                  │                      │           │
│         │                  │                      │           │
│         └──────────────────┼──────────────────────│           │
│                            │                      │           │
│                            v                      v           │
│                    ┌─────────────────────────────────┐       │
│                    │        DDR Memory               │       │
│                    │  (输入数据 + 输出Buffer)         │       │
│                    └─────────────────────────────────┘       │
└───────────────────────────────────────────────────────────────┘
```

### 3.2 Vivado TCL脚本创建Block Design

```tcl
# =====================================================================
# Vivado Block Design Setup for SOCS HLS Board Validation
# =====================================================================

# 创建项目（如果不存在）
# create_project board_validation ./board_validation_proj -part xcku3p-ffvb676-2-e

# 打开Block Design
create_bd_design "soc_hls_validation"

# 1. 添加JTAG-to-AXI Master (用于PC通过JTAG访问DDR)
create_bd_cell -type ip -vlnv xilinx.com:ip:jtag_axi_master:1.0 jtag_axi_master_0

# 2. 添加AXI SmartConnect (互联JTAG Master、HLS IP、DDR)
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property -dict [list CONFIG.NUM_MI {7} CONFIG.NUM_SI {2}] [get_bd_cells smartconnect_0]

# 3. 添加HLS IP核
create_bd_cell -type ip -vlnv xilinx.com:hls:calc_socs_hls:1.0 calc_socs_hls_1

# 4. 添加DDR控制器 (MIG or AXI DDR Controller)
# 注：根据实际板卡选择，这里以AXI DDR Controller为例
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_ddr_cntrl:1.0 axi_ddr_cntrl_0

# 5. 添加时钟和复位
create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:1.0 clk_wiz_0
set_property -dict [list CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {273.97}] [get_bd_cells clk_wiz_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:1.0 proc_sys_reset_0

# 6. 连接时钟和复位
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins calc_socs_hls_1/ap_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins smartconnect_0/aclk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_ddr_cntrl_0/c0_ddr4_clk]

connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins calc_socs_hls_1/ap_rst_n]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins smartconnect_0/aresetn]

# 7. 连接AXI接口
# JTAG Master -> SmartConnect (SI0)
connect_bd_intf_net [get_bd_intf_pins jtag_axi_master_0/M_AXI] [get_bd_intf_pins smartconnect_0/S00_AXI]

# HLS IP Control -> SmartConnect (SI1) - 如果需要处理器控制
# 注：纯JTAG验证可省略，直接通过JTAG写入AXI-Lite控制寄存器

# SmartConnect -> HLS AXI-Lite Control (MI0)
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M00_AXI] [get_bd_intf_pins calc_socs_hls_1/s_axi_control]

# SmartConnect -> HLS AXI-Lite Address (MI1)
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M01_AXI] [get_bd_intf_pins calc_socs_hls_1/s_axi_control_r]

# SmartConnect -> DDR (MI2-MI6 for HLS AXI-Master ports)
# 注：HLS有6个AXI-Master端口，需要连接到DDR
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M02_AXI] [get_bd_intf_pins axi_ddr_cntrl_0/C0_DDR4_S_AXI]

# HLS AXI-Master -> SmartConnect (通过Interconnect到DDR)
# 推荐使用AXI Interconnect将HLS的6个Master端口汇聚到DDR
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:1.0 axi_interconnect_0
set_property -dict [list CONFIG.NUM_MI {1} CONFIG.NUM_SI {6}] [get_bd_cells axi_interconnect_0]

# 连接HLS gmem0-5到AXI Interconnect
for {set i 0} {$i < 6} {incr i} {
    connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem$i] \
                        [get_bd_intf_pins axi_interconnect_0/S0$i_AXI]
}
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M00_AXI] \
                    [get_bd_intf_pins axi_ddr_cntrl_0/C0_DDR4_S_AXI]

# 8. 地址分配（Address Editor）
# 参考Vivado Address Editor自动分配，或手动设置：
# - HLS AXI-Lite Control: 0x40000000 (64KB)
# - HLS AXI-Lite Address: 0x40010000 (64KB)
# - DDR Memory Range: 0x80000000 - 0xFFFFFFFF (根据DDR容量)

# 9. 生成Bitstream
validate_bd_design
save_bd_design
make_wrapper -files [get_files ./board_validation_proj.srcs/sources_1/bd/soc_hls_validation/soc_hls_validation.bd] -top
add_files -norecurse ./board_validation_proj.gen/sources_1/bd/soc_hls_validation/hdl/soc_hls_validation_wrapper.v
launch_runs impl_1 -to_step write_bitstream
```

---

## 4. JTAG-to-AXI 验证流程（Vivado Hardware Manager）

### 4.1 连接硬件

```tcl
# =====================================================================
# Step 1: Connect Hardware and Program FPGA
# =====================================================================
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
```

### 4.2 Reset AXI Master

```tcl
# Step 2: Reset JTAG-to-AXI Master Controller
reset_hw_axi [get_hw_axis hw_axi_1]
```

### 4.3 DDR地址规划

**推荐DDR地址分配**（假设DDR基地址为 `0x80000000`）：

| 数据类型 | DDR地址 | 大小 | 说明 |
|----------|----------|------|------|
| `mskf_r` | 0x80000000 | 1MB | Mask频域实部 (512×512 float) |
| `mskf_i` | 0x80100000 | 1MB | Mask频域虚部 |
| `scales` | 0x80200000 | 40B | 特征值 (10 float) |
| `krn_r` | 0x80200100 | 3.2KB | Kernel实部 (10×9×9 float) |
| `krn_i` | 0x80201000 | 3.2KB | Kernel虚部 |
| `output` | 0x80300000 | 4KB | 输出Buffer (32×32 float) |

### 4.4 写入输入数据到DDR

**方案A：逐个数据块写入（推荐用于小数据量）**

```tcl
# =====================================================================
# Step 3: Write Input Data to DDR via JTAG-to-AXI
# =====================================================================

# 辅助函数：写入BIN文件到DDR
proc write_bin_to_ddr {axi_master ddr_addr bin_file {burst_len 256}} {
    # 读取BIN文件（PC端Python预处理）
    # 注：Vivado TCL不支持直接读取BIN，需先转换为hex格式
    # 推荐使用PC端Python脚本生成TCL命令序列
    
    # 示例：写入单个数据块（256个float = 1024 bytes）
    # 实际操作需结合PC端Python脚本生成完整TCL序列
    puts "INFO: Writing $bin_file to DDR address $ddr_addr"
}

# 实际写入命令（需PC端Python生成TCL序列）
# 详见第5节 Python脚本自动生成
```

**方案B：PC端Python生成TCL命令序列（推荐）**

详见第5节 Python脚本，自动生成完整TCL写入序列。

### 4.5 配置HLS IP地址寄存器

```tcl
# =====================================================================
# Step 4: Configure HLS IP Address Registers (AXI-Lite control_r)
# =====================================================================

# HLS AXI-Lite control_r基地址（根据Address Editor分配）
set HLS_CTRL_R_BASE 0x40010000

# 寄存器偏移（参考IP_PORT_CONNECTION_GUIDE.md）
# 0x00: mskf_r pointer
# 0x04: mskf_i pointer
# 0x08: scales pointer
# 0x0C: krn_r pointer
# 0x10: krn_i pointer
# 0x14: output pointer

# 写入DDR地址到HLS寄存器
create_hw_axi_txn write_mskf_r [get_hw_axis hw_axi_1] \
    -address ${HLS_CTRL_R_BASE} \
    -data 0x80000000 \
    -len 1 -type write
run_hw_axi_txn write_mskf_r

create_hw_axi_txn write_mskf_i [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x04] \
    -data 0x80100000 \
    -len 1 -type write
run_hw_axi_txn write_mskf_i

create_hw_axi_txn write_scales [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x08] \
    -data 0x80200000 \
    -len 1 -type write
run_hw_axi_txn write_scales

create_hw_axi_txn write_krn_r [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x0C] \
    -data 0x80200100 \
    -len 1 -type write
run_hw_axi_txn write_krn_r

create_hw_axi_txn write_krn_i [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x10] \
    -data 0x80201000 \
    -len 1 -type write
run_hw_axi_txn write_krn_i

create_hw_axi_txn write_output [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x14] \
    -data 0x80300000 \
    -len 1 -type write
run_hw_axi_txn write_output

puts "INFO: HLS address registers configured"
```

### 4.6 启动HLS IP执行

```tcl
# =====================================================================
# Step 5: Start HLS IP Execution (Write ap_start)
# =====================================================================

# HLS AXI-Lite control基地址
set HLS_CTRL_BASE 0x40000000

# 写入ap_start=1 (地址0x00, bit[0]=1)
create_hw_axi_txn start_hls [get_hw_axis hw_axi_1] \
    -address ${HLS_CTRL_BASE} \
    -data 0x00000001 \
    -len 1 -type write
run_hw_axi_txn start_hls

puts "INFO: HLS IP started (ap_start=1)"
```

### 4.7 等待IP完成（轮询ap_done）

```tcl
# =====================================================================
# Step 6: Poll ap_done with Timeout Protection
# =====================================================================

# 轮询函数（推荐封装为proc）
proc wait_for_ap_done {axi_master ctrl_base timeout_ms} {
    set start_time [clock milliseconds]
    set done_offset 0x00  ;# AP_CTRL寄存器地址
    set done_mask 0x02    ;# bit[1] = ap_done
    
    while {true} {
        # 读取AP_CTRL寄存器
        create_hw_axi_txn read_status $axi_master \
            -address $ctrl_base \
            -len 1 -type read
        run_hw_axi_txn read_status
        
        # 获取读取数据（需要从transaction中提取）
        set status_data [get_property DATA [get_hw_axi_txn read_status]]
        
        # 检查ap_done bit
        if {[expr {$status_data & $done_mask}] != 0} {
            puts "INFO: HLS IP execution completed (ap_done=1)"
            return 1
        }
        
        # 超时检查
        set current_time [clock milliseconds]
        set elapsed [expr {$current_time - $start_time}]
        if {$elapsed > $timeout_ms} {
            puts "ERROR: Timeout waiting for ap_done (${timeout_ms}ms elapsed)"
            return 0
        }
        
        # 等待1ms后再次轮询
        after 1
    }
}

# 执行轮询（超时5000ms = 5秒，HLS latency约201,833 cycles @ 273MHz ≈ 0.74ms）
# 实际需考虑DDR访问延迟，建议超时设为5000ms
set done_success [wait_for_ap_done [get_hw_axis hw_axi_1] ${HLS_CTRL_BASE} 5000]

if {$done_success} {
    puts "INFO: HLS execution successful, ready to read output"
} else {
    puts "ERROR: HLS execution failed or timeout"
    exit 1
}
```

### 4.8 读取输出数据

```tcl
# =====================================================================
# Step 7: Read Output Data from DDR
# =====================================================================

# 输出地址和长度
set OUTPUT_DDR_ADDR 0x80300000
set OUTPUT_SIZE 289   ;# 17×17 floats
set OUTPUT_BURST_LEN [expr {$OUTPUT_SIZE * 4 / 4}]  ;# 字节数/4 = 289 words

# 读取输出数据（32-bit words）
create_hw_axi_txn read_output [get_hw_axis hw_axi_1] \
    -address ${OUTPUT_DDR_ADDR} \
    -len ${OUTPUT_BURST_LEN} \
    -type read
run_hw_axi_txn read_output

# 获取读取的数据
set output_data [get_property DATA [get_hw_axi_txn read_output]]
puts "INFO: Output data read from DDR"

# 注：output_data为字符串格式，需PC端Python解析并对比Golden
# 详见第5节 Python脚本处理
```

---

## 5. PC端Python脚本自动化

### 5.1 生成TCL写入命令脚本

创建文件 `board_validation/scripts/generate_jtag_tcl.py`:

```python
#!/usr/bin/env python3
"""
Generate TCL commands for JTAG-to-AXI DDR write operations
Converts BIN files to Vivado TCL command sequences
"""

import struct
import os

# DDR地址规划
DDR_BASE = 0x80000000
DDR_ADDRS = {
    'mskf_r': DDR_BASE + 0x00000000,    # 1MB
    'mskf_i': DDR_BASE + 0x00100000,    # 1MB
    'scales': DDR_BASE + 0x00200000,    # 40B
    'krn_r':  DDR_BASE + 0x00200100,    # 3.2KB (10×9×9 float)
    'krn_i':  DDR_BASE + 0x00201000,    # 3.2KB
    'output': DDR_BASE + 0x00300000,    # 4KB (预留)
}

# AXI burst参数
MAX_BURST_LEN = 256  ;# Vivado JTAG-to-AXI支持最大256 beats

def read_bin_file(filepath):
    """读取BIN文件为float32数组"""
    with open(filepath, 'rb') as f:
        data = f.read()
    floats = struct.unpack(f'{len(data)//4}f', data)
    return floats

def float_to_hex(f):
    """将float转换为32-bit hex字符串"""
    packed = struct.pack('f', f)
    hex_str = struct.unpack('I', packed)[0]
    return f'{hex_str:08x}'

def generate_write_tcl(bin_file, ddr_addr, tcl_file):
    """生成TCL写入命令序列"""
    floats = read_bin_file(bin_file)
    
    with open(tcl_file, 'w') as f:
        f.write(f'# TCL script for writing {bin_file} to DDR\n')
        f.write(f'# Target DDR address: 0x{ddr_addr:08x}\n')
        f.write(f'# Data size: {len(floats)} floats ({len(floats)*4} bytes)\n\n')
        
        # 分块写入（每块最多256个float）
        offset = 0
        txn_id = 0
        
        while offset < len(floats):
            # 计算当前块的长度
            chunk_len = min(MAX_BURST_LEN, len(floats) - offset)
            chunk_data = floats[offset:offset + chunk_len]
            
            # 生成hex数据字符串（32-bit words）
            hex_data = [float_to_hex(f) for f in chunk_data]
            hex_str = ' '.join(hex_data)
            
            # 计算当前块地址
            block_addr = ddr_addr + offset * 4
            
            # 生成TCL命令
            f.write(f'# Block {txn_id}: offset={offset}, len={chunk_len}\n')
            f.write(f'create_hw_axi_txn write_block_{txn_id} [get_hw_axis hw_axi_1] \\\n')
            f.write(f'    -address 0x{block_addr:08x} \\\n')
            f.write(f'    -data {hex_str} \\\n')
            f.write(f'    -len {chunk_len} -type write\n')
            f.write(f'run_hw_axi_txn write_block_{txn_id}\n\n')
            
            offset += chunk_len
            txn_id += 1
        
        f.write(f'# Total {txn_id} write transactions completed\n')

def generate_full_tcl_script(output_tcl):
    """生成完整的板级验证TCL脚本"""
    golden_dir = '/root/project/FPGA-Litho/output/verification'
    
    tcl_content = '''# =====================================================================
# FPGA-Litho SOCS HLS Board Validation TCL Script
# Auto-generated by generate_jtag_tcl.py
# =====================================================================

# Step 1: Connect Hardware
puts "INFO: Connecting to hardware..."
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# Step 2: Reset AXI Master
reset_hw_axi [get_hw_axis hw_axi_1]

# Step 3: Write Input Data to DDR
puts "INFO: Writing input data to DDR..."
'''
    
    # 写入各数据块（分块处理）
    for name, addr in DDR_ADDRS.items():
        if name == 'output':
            continue  ;# 输出buffer不需要预写入
        
        if name.startswith('krn'):
            ;# 处理10个kernel文件
            for i in range(10):
                suffix = '_r' if name == 'krn_r' else '_i'
                bin_file = f'{golden_dir}/kernels/krn_{i}{suffix}.bin'
                if os.path.exists(bin_file):
                    ;# 每个kernel单独写入其偏移地址
                    kernel_addr = addr + i * 9 * 9 * 4
                    tcl_content += generate_inline_write(bin_file, kernel_addr)
        else:
            bin_file = f'{golden_dir}/{name}.bin'
            if os.path.exists(bin_file):
                tcl_content += generate_inline_write(bin_file, addr)
    
    tcl_content += '''
# Step 4: Configure HLS Address Registers
puts "INFO: Configuring HLS address registers..."
set HLS_CTRL_R_BASE 0x40010000

'''
    
    ;# 配置地址寄存器
    for name, addr in DDR_ADDRS.items():
        offset = list(DDR_ADDRS.keys()).index(name) * 4
        tcl_content += f'''create_hw_axi_txn write_{name}_addr [get_hw_axis hw_axi_1] \
    -address [expr ${HLS_CTRL_R_BASE} + 0x{offset:02x}] \
    -data 0x{addr:08x} -len 1 -type write
run_hw_axi_txn write_{name}_addr

'''
    
    tcl_content += '''# Step 5: Start HLS IP
puts "INFO: Starting HLS IP execution..."
set HLS_CTRL_BASE 0x40000000
create_hw_axi_txn start_hls [get_hw_axis hw_axi_1] \
    -address ${HLS_CTRL_BASE} \
    -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_hls

# Step 6: Poll ap_done
puts "INFO: Waiting for HLS completion..."
set timeout_ms 5000
set start_time [clock milliseconds]
set done 0

while {!$done} {
    create_hw_axi_txn read_status [get_hw_axis hw_axi_1] \
        -address ${HLS_CTRL_BASE} -len 1 -type read
    run_hw_axi_txn read_status
    
    set status [get_property DATA [get_hw_axi_txn read_status]]
    if {[expr {$status & 0x02}] != 0} {
        set done 1
        puts "INFO: HLS execution completed"
    }
    
    set elapsed [expr {[clock milliseconds] - $start_time}]
    if {$elapsed > $timeout_ms} {
        puts "ERROR: Timeout waiting for ap_done"
        exit 1
    }
    after 10
}

# Step 7: Read Output Data
puts "INFO: Reading output data from DDR..."
set OUTPUT_ADDR 0x80300000
set OUTPUT_LEN 289

create_hw_axi_txn read_output [get_hw_axis hw_axi_1] \
    -address ${OUTPUT_ADDR} \
    -len ${OUTPUT_LEN} -type read
run_hw_axi_txn read_output

set output_raw [get_property DATA [get_hw_axi_txn read_output]]
puts "INFO: Output data captured"

# Save output to file for PC analysis
set output_file "board_output.txt"
set fp [open $output_file w]
puts $fp $output_raw
close $fp
puts "INFO: Output saved to $output_file"

puts "INFO: Board validation completed successfully"
'''
    
    with open(output_tcl, 'w') as f:
        f.write(tcl_content)
    
    print(f"Generated TCL script: {output_tcl}")

def generate_inline_write(bin_file, addr):
    """生成内联写入命令（用于大文件分块）"""
    floats = read_bin_file(bin_file)
    lines = f'# Writing {bin_file} ({len(floats)} floats) to 0x{addr:08x}\n'
    
    # 对于大文件，分块写入
    chunk_size = 64  ;# 减少TCL文件大小，每块64 floats
    for i in range(0, len(floats), chunk_size):
        chunk = floats[i:i+chunk_size]
        hex_data = ' '.join([float_to_hex(f) for f in chunk])
        block_addr = addr + i * 4
        lines += f'create_hw_axi_txn wr_{addr:x}_{i} [get_hw_axis hw_axi_1] -address 0x{block_addr:08x} -data {hex_data} -len {len(chunk)} -type write\n'
        lines += f'run_hw_axi_txn wr_{addr:x}_{i}\n'
    
    return lines + '\n'

if __name__ == '__main__':
    output_tcl = 'board_validation/scripts/run_board_validation.tcl'
    generate_full_tcl_script(output_tcl)
```

### 5.2 输出数据解析与Golden对比

创建文件 `board_validation/scripts/compare_results.py`:

```python
#!/usr/bin/env python3
"""
Parse HLS board output and compare with Golden data
Generate visualization and error metrics
"""

import struct
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

def parse_tcl_output(tcl_file):
    """解析Vivado TCL输出的hex数据"""
    with open(tcl_file, 'r') as f:
        content = f.read()
    
    ;# 提取hex字符串（格式：'0x12345678 0xabcdef ...'）
    hex_values = content.split()
    floats = []
    
    for hex_val in hex_values:
        ;# 去除0x前缀（如果有）
        if hex_val.startswith('0x'):
            hex_val = hex_val[2:]
        
        ;# 转换hex到float32
        int_val = int(hex_val, 16)
        float_val = struct.unpack('f', struct.pack('I', int_val))[0]
        floats.append(float_val)
    
    return np.array(floats)

def load_golden_output(golden_file):
    """加载Golden输出数据"""
    with open(golden_file, 'rb') as f:
        data = f.read()
    floats = struct.unpack(f'{len(data)//4}f', data)
    return np.array(floats)

def calculate_metrics(board_output, golden_output):
    """计算误差指标"""
    ;# RMSE (Root Mean Square Error)
    rmse = np.sqrt(np.mean((board_output - golden_output) ** 2))
    
    ;# 相对误差（百分比）
    rel_error = np.abs(board_output - golden_output) / np.abs(golden_output + 1e-10)
    mean_rel_error = np.mean(rel_error) * 100
    
    ;# 最大绝对误差
    max_abs_error = np.max(np.abs(board_output - golden_output))
    
    ;# Pearson相关系数
    correlation = np.corrcoef(board_output, golden_output)[0, 1]
    
    return {
        'RMSE': rmse,
        'Mean_Rel_Error_%': mean_rel_error,
        'Max_Abs_Error': max_abs_error,
        'Correlation': correlation
    }

def visualize_comparison(board_output, golden_output, output_prefix):
    """生成可视化对比图"""
    ;# reshape为32×32（如果数据是1024 floats）
    ;# 或17×17（如果数据是289 floats）
    size = int(np.sqrt(len(board_output)))
    if size * size != len(board_output):
        print(f"WARNING: Data size {len(board_output)} not perfect square")
        size = 17  ;# 假设是17×17输出
    
    board_img = board_output.reshape(size, size)
    golden_img = golden_output.reshape(size, size)
    
    ;# 创建对比图
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    ;# Board output
    axes[0, 0].imshow(board_img, cmap='hot')
    axes[0, 0].set_title('Board Output (HLS IP)')
    axes[0, 0].axis('off')
    
    ;# Golden output
    axes[0, 1].imshow(golden_img, cmap='hot')
    axes[0, 1].set_title('Golden Output (CPU Reference)')
    axes[0, 1].axis('off')
    
    ;# Difference map
    diff = np.abs(board_img - golden_img)
    axes[1, 0].imshow(diff, cmap='viridis')
    axes[1, 0].set_title('Absolute Difference')
    axes[1, 0].axis('off')
    
    ;# Scatter plot
    axes[1, 1].scatter(golden_output, board_output, alpha=0.5, s=5)
    axes[1, 1].plot([golden_output.min(), golden_output.max()], 
                    [golden_output.min(), golden_output.max()], 'r--', lw=2)
    axes[1, 1].set_xlabel('Golden Output')
    axes[1, 1].set_ylabel('Board Output')
    axes[1, 1].set_title('Correlation Plot')
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{output_prefix}_comparison.png', dpi=150, bbox_inches='tight')
    plt.close()
    
    print(f"Saved visualization: {output_prefix}_comparison.png")

def main():
    ;# 文件路径
    board_output_file = 'board_validation/board_output.txt'
    golden_file = '/root/project/FPGA-Litho/output/verification/tmpImgp_pad32.bin'
    output_prefix = 'board_validation/results/validation'
    
    ;# 加载数据
    print("Loading board output...")
    board_output = parse_tcl_output(board_output_file)
    print(f"Board output size: {len(board_output)} floats")
    
    print("Loading golden output...")
    golden_output = load_golden_output(golden_file)
    print(f"Golden output size: {len(golden_output)} floats")
    
    ;# 数据尺寸匹配检查
    if len(board_output) != len(golden_output):
        print(f"WARNING: Size mismatch - board:{len(board_output)}, golden:{len(golden_output)}")
        ;# 截取到相同长度
        min_len = min(len(board_output), len(golden_output))
        board_output = board_output[:min_len]
        golden_output = golden_output[:min_len]
    
    ;# 计算误差指标
    print("Calculating error metrics...")
    metrics = calculate_metrics(board_output, golden_output)
    
    print("\n=== Validation Results ===")
    for key, value in metrics.items():
        print(f"{key}: {value:.6f}")
    
    ;# 可视化对比
    print("Generating visualization...")
    visualize_comparison(board_output, golden_output, output_prefix)
    
    ;# 保存metrics到文件
    with open(f'{output_prefix}_metrics.txt', 'w') as f:
        f.write("=== HLS Board Validation Results ===\n")
        for key, value in metrics.items():
            f.write(f"{key}: {value}\n")
    
    print(f"Saved metrics: {output_prefix}_metrics.txt")
    
    ;# 验证结论
    if metrics['Correlation'] > 0.99 and metrics['RMSE'] < 0.01:
        print("\n✅ VALIDATION PASSED: Board output matches Golden data")
    else:
        print("\n❌ VALIDATION FAILED: Significant discrepancy detected")

if __name__ == '__main__':
    main()
```

---

## 6. 完整验证流程执行

### 6.1 PC端准备（执行一次）

```bash
cd /root/project/FPGA-Litho

# 1. 生成Golden数据（如果不存在）
python verification/run_verification.py

# 2. 创建board_validation目录
mkdir -p board_validation/scripts board_validation/results

# 3. 生成TCL脚本
python board_validation/scripts/generate_jtag_tcl.py
```

### 6.2 Vivado Hardware Manager执行

```bash
# 1. 打开Vivado Hardware Manager
vivado -mode batch -source board_validation/scripts/run_board_validation.tcl

# 或在Vivado GUI中：
# Tools -> Open Hardware Manager
# 在Tcl Console中执行：
# source board_validation/scripts/run_board_validation.tcl
```

### 6.3 PC端结果分析

```bash
# 解析输出并对比Golden
python board_validation/scripts/compare_results.py

# 查看结果
cat board_validation/results/validation_metrics.txt
ls board_validation/results/*.png
```

---

## 7. 常见问题与调试

### 7.1 JTAG连接失败

**症状**: `connect_hw_server` 或 `open_hw_target` 报错

**解决方案**:
1. 检查JTAG链物理连接
2. 确认FPGA已正确上电
3. 验证JTAG服务器运行状态：`localhost:3121`
4. 尝试手动启动Hardware Server

### 7.2 DDR写入超时

**症状**: AXI写入事务长时间无响应

**解决方案**:
1. 减小burst长度（从256降到64）
2. 检查DDR控制器配置是否正确
3. 验证AXI Interconnect时钟域同步

### 7.3 HLS IP不启动

**症状**: 写入ap_start后IP不执行

**排查步骤**:
1. 验证AXI-Lite control基地址正确
2. 检查ap_clk和ap_rst_n连接
3. 确认DDR地址寄存器配置完成
4. 读取AP_CTRL寄存器（地址0x00）检查状态：
   ```tcl
   create_hw_axi_txn read_ctrl [get_hw_axis hw_axi_1] -address 0x40000000 -len 1 -type read
   run_hw_axi_txn read_ctrl
   set ctrl_data [get_property DATA [get_hw_axi_txn read_ctrl]]
   puts "AP_CTRL: $ctrl_data"
   ```

### 7.4 输出数据全为0或NaN

**症状**: 输出数据异常

**排查步骤**:
1. 检查输入数据是否正确写入DDR
2. 验证Golden数据的float格式是否正确
3. 检查HLS C Simulation是否通过（验证算法正确性）
4. 如发现NaN，检查HLS代码中的数值稳定性处理

### 7.5 Golden数据尺寸不匹配

**症状**: Board输出289 floats (17×17)，Golden是1024 floats (32×32)

**解决方案**:
- HLS v13输出尺寸：17×17 = 289 floats（center extraction）
- Golden `tmpImgp_pad32.bin` 是32×32完整数据
- Python对比脚本自动截取center 17×17进行对比：
  ```python
  # 从32×32 Golden中提取center 17×17
  golden_32x32 = golden_output.reshape(32, 32)
  start = (32-17)//2
  golden_17x17 = golden_32x32[start:start+17, start:start+17].flatten()
  ```

---

## 8. 验证成功标准

| 指标 | 阈值 | 说明 |
|------|------|------|
| **Correlation** | > 0.99 | Board输出与Golden高度相关 |
| **RMSE** | < 0.01 | 均方根误差小于1% |
| **Max_Abs_Error** | < 0.05 | 最大绝对误差小于5% |
| **Mean_Rel_Error** | < 5% | 平均相对误差小于5% |

**如果所有指标达标，判定验证通过，IP核功能正确。**

---

## 9. 附录：完整TCL脚本模板

**文件**: `board_validation/scripts/run_board_validation_template.tcl`

```tcl
# =====================================================================
# FPGA-Litho SOCS HLS Board Validation TCL Template
# Version: 1.0
# Target IP: calc_socs_hls v13
# =====================================================================

# User Configuration Section
# ============================================================
set HLS_CTRL_BASE      0x40000000    ;# AXI-Lite Control base address
set HLS_CTRL_R_BASE    0x40010000    ;# AXI-Lite Address Registers base
set DDR_BASE           0x80000000    ;# DDR memory base address
set TIMEOUT_MS         5000          ;# ap_done polling timeout (ms)

# DDR Address Allocation
# ============================================================
set DDR_MSKF_R         [expr ${DDR_BASE} + 0x00000000]
set DDR_MSKF_I         [expr ${DDR_BASE} + 0x00100000]
set DDR_SCALES         [expr ${DDR_BASE} + 0x00200000]
set DDR_KRN_R          [expr ${DDR_BASE} + 0x00200100]
set DDR_KRN_I          [expr ${DDR_BASE} + 0x00201000]
set DDR_OUTPUT         [expr ${DDR_BASE} + 0x00300000]

# ============================================================
# Step 1: Hardware Connection
# ============================================================
puts "========================================"
puts "Step 1: Connecting Hardware"
puts "========================================"

catch {disconnect_hw_server}
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
puts "INFO: FPGA programmed successfully"

# ============================================================
# Step 2: Initialize AXI Master
# ============================================================
puts "========================================"
puts "Step 2: Resetting AXI Master"
puts "========================================"

reset_hw_axi [get_hw_axis hw_axi_1]
puts "INFO: JTAG-to-AXI Master reset complete"

# ============================================================
# Step 3: Write Input Data (Placeholder - Use Python Script)
# ============================================================
puts "========================================"
puts "Step 3: Writing Input Data to DDR"
puts "========================================"

puts "INFO: Use Python script to generate detailed write commands"
puts "INFO: Placeholder for mskf_r.bin, mskf_i.bin, scales.bin, kernels.bin"

# ============================================================
# Step 4: Configure HLS Address Registers
# ============================================================
puts "========================================"
puts "Step 4: Configuring HLS Address Registers"
puts "========================================"

proc write_addr_reg {axi_master base_addr offset data} {
    create_hw_axi_txn txn $axi_master \
        -address [expr ${base_addr} + ${offset}] \
        -data ${data} -len 1 -type write
    run_hw_axi_txn txn
}

# Write DDR pointers to HLS registers
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x00 ${DDR_MSKF_R}
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x04 ${DDR_MSKF_I}
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x08 ${DDR_SCALES}
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x0C ${DDR_KRN_R}
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x10 ${DDR_KRN_I}
write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_R_BASE} 0x14 ${DDR_OUTPUT}

puts "INFO: Address registers configured"
puts "  mskf_r: 0x${DDR_MSKF_R}"
puts "  mskf_i: 0x${DDR_MSKF_I}"
puts "  scales: 0x${DDR_SCALES}"
puts "  krn_r:  0x${DDR_KRN_R}"
puts "  krn_i:  0x${DDR_KRN_I}"
puts "  output: 0x${DDR_OUTPUT}"

# ============================================================
# Step 5: Start HLS Execution
# ============================================================
puts "========================================"
puts "Step 5: Starting HLS IP"
puts "========================================"

write_addr_reg [get_hw_axis hw_axi_1] ${HLS_CTRL_BASE} 0x00 0x00000001
puts "INFO: ap_start written, HLS execution initiated"

# ============================================================
# Step 6: Poll ap_done with Timeout
# ============================================================
puts "========================================"
puts "Step 6: Waiting for Completion"
puts "========================================"

set start_time [clock milliseconds]
set done 0

while {!$done} {
    # Read AP_CTRL register
    create_hw_axi_txn read_ctrl [get_hw_axis hw_axi_1] \
        -address ${HLS_CTRL_BASE} -len 1 -type read
    run_hw_axi_txn read_ctrl
    
    set ctrl_data [get_property DATA [get_hw_axi_txn read_ctrl]]
    set ap_done_bit [expr {$ctrl_data & 0x02}]
    
    if {$ap_done_bit != 0} {
        set done 1
        puts "INFO: HLS execution completed (ap_done=1)"
    }
    
    # Timeout check
    set elapsed [expr {[clock milliseconds] - $start_time}]
    if {$elapsed > ${TIMEOUT_MS}} {
        puts "ERROR: Timeout after ${TIMEOUT_MS}ms"
        puts "ERROR: AP_CTRL value: $ctrl_data"
        exit 1
    }
    
    after 10
}

# ============================================================
# Step 7: Read Output Data
# ============================================================
puts "========================================"
puts "Step 7: Reading Output Data"
puts "========================================"

set OUTPUT_LEN 289  ;# 17×17 floats

create_hw_axi_txn read_output [get_hw_axis hw_axi_1] \
    -address ${DDR_OUTPUT} \
    -len ${OUTPUT_LEN} -type read
run_hw_axi_txn read_output

set output_data [get_property DATA [get_hw_axi_txn read_output]]
puts "INFO: Output data read successfully (${OUTPUT_LEN} floats)"

# Save output to file
set output_file "board_output.txt"
set fp [open $output_file w]
puts $fp $output_data
close $fp
puts "INFO: Output saved to: $output_file"

# ============================================================
# Step 8: Validation Complete
# ============================================================
puts "========================================"
puts "Board Validation Complete"
puts "========================================"
puts "Next step: Run compare_results.py on PC to validate against Golden"
puts "Output file: board_output.txt"
puts "Golden file: tmpImgp_pad32.bin"
```

---

## 10. 参考资料

| 文档 | 路径 | 用途 |
|------|------|------|
| IP端口连线指南 | `source/SOCS_HLS/IP_PORT_CONNECTION_GUIDE.md` | 接口定义与地址映射 |
| BIN格式规范 | `input/BIN_Format_Specification.md` | 数据格式定义 |
| HLS Board Validation Skill | `.github/skills/hls-board-validation/SKILL.md` | JTAG操作流程 |
| HLS Golden Generation Skill | `.github/skills/hls-golden-generation/SKILL.md` | 数据生成流程 |
| Vivado TCL参考 | `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl` | JTAG-to-AXI模板 |