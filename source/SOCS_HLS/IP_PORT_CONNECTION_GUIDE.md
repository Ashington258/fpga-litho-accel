# calc_socs_hls IP核端口连线指南

**生成版本**: v10 (socs_full_csynth_v10)
**IP路径**: `source/SOCS_HLS/socs_full_csynth_v10/hls/impl/ip/xilinx_com_hls_calc_socs_hls_1_0.zip`
**目标器件**: xcku3p-ffvb676-2-e (Kintex UltraScale+)
**性能指标**: Fmax 273.97 MHz, Latency 201,833 cycles

---

## 1. IP核接口总览

该IP核包含以下主要接口类型：

| 接口类型 | 接口名称 | 数量 | 用途 |
|---------|---------|------|------|
| **AXI4-Lite Slave** | `s_axi_control` | 1 | 控制寄存器（启动/状态） |
| **AXI4-Lite Slave** | `s_axi_control_r` | 1 | 参数地址寄存器 |
| **AXI4-Master** | `m_axi_gmem[0-5]` | 6 | DDR内存访问（数据传输） |
| **Clock** | `ap_clk` | 1 | 主时钟输入 |
| **Reset** | `ap_rst_n` | 1 | 低电平复位 |
| **Interrupt** | `interrupt` | 1 | 完成中断信号 |

---

## 2. 详细端口列表与连线需求

### 2.1 时钟与复位端口

| 端口名 | 方向 | 位宽 | 连线目标 | 说明 |
|--------|------|------|----------|------|
| `ap_clk` | Input | 1 | 系统主时钟（推荐100-300 MHz） | **必须连接**，建议使用 PLL/MMCM 生成 273.97 MHz 以达到最优性能 |
| `ap_rst_n` | Input | 1 | 系统复位信号（低电平有效） | **必须连接**，通常接处理器系统的 `perst_n` 或独立复位控制器 |

**连线建议**：
```tcl
# Vivado Block Design 示例
connect_bd_net [get_bd_pins clk_wiz/clk_out1] [get_bd_pins calc_socs_hls_1/ap_clk]
connect_bd_net [get_bd_pins proc_sys_reset/peripheral_aresetn] [get_bd_pins calc_socs_hls_1/ap_rst_n]
```

---

### 2.2 AXI4-Lite Slave 控制接口（s_axi_control）

**用途**: 控制 IP 启动、查询状态、配置中断

| 端口名 | 方向 | 位宽 | AXI信号类型 | 连线目标 |
|--------|------|------|------------|----------|
| `s_axi_control_AWVALID` | Input | 1 | Write Address Valid | 处理器 AXI-Lite Master |
| `s_axi_control_AWREADY` | Output | 1 | Write Address Ready | 处理器 AXI-Lite Master |
| `s_axi_control_AWADDR` | Input | 4 | Write Address | 处理器 AXI-Lite Master |
| `s_axi_control_WVALID` | Input | 1 | Write Data Valid | 处理器 AXI-Lite Master |
| `s_axi_control_WREADY` | Output | 1 | Write Data Ready | 处理器 AXI-Lite Master |
| `s_axi_control_WDATA` | Input | 32 | Write Data | 处理器 AXI-Lite Master |
| `s_axi_control_WSTRB` | Input | 4 | Write Strobe | 处理器 AXI-Lite Master |
| `s_axi_control_ARVALID` | Input | 1 | Read Address Valid | 处理器 AXI-Lite Master |
| `s_axi_control_ARREADY` | Output | 1 | Read Address Ready | 处理器 AXI-Lite Master |
| `s_axi_control_ARADDR` | Input | 4 | Read Address | 处理器 AXI-Lite Master |
| `s_axi_control_RVALID` | Output | 1 | Read Data Valid | 处理器 AXI-Lite Master |
| `s_axi_control_RREADY` | Input | 1 | Read Data Ready | 处理器 AXI-Lite Master |
| `s_axi_control_RDATA` | Output | 32 | Read Data | 处理器 AXI-Lite Master |
| `s_axi_control_RRESP` | Output | 2 | Read Response | 处理器 AXI-Lite Master |
| `s_axi_control_BVALID` | Output | 1 | Write Response Valid | 处理器 AXI-Lite Master |
| `s_axi_control_BREADY` | Input | 1 | Write Response Ready | 处理器 AXI-Lite Master |
| `s_axi_control_BRESP` | Output | 2 | Write Response | 处理器 AXI-Lite Master |

**控制寄存器映射**：

| 地址偏移 | 寄存器名称 | 位定义 | 功能 |
|----------|-----------|--------|------|
| `0x00` | AP_CTRL | bit0: ap_start (RW)<br>bit1: ap_done (R)<br>bit2: ap_idle (R)<br>bit3: ap_ready (R)<br>bit7: auto_restart (RW)<br>bit9: interrupt (R) | **核心控制寄存器**：写 `0x01` 启动 IP，轮询 `bit1` 查询完成 |
| `0x04` | GIE | bit0: Global Interrupt Enable | 全局中断使能 |
| `0x08` | IER | bit0: ap_done IRQ enable<br>bit1: ap_ready IRQ enable | 中断使能寄存器 |
| `0x0C` | ISR | bit0: ap_done (Read/Toggle)<br>bit1: ap_ready (Read/Toggle) | 中断状态寄存器 |

**连线建议**：
```tcl
# 在 Vivado Block Design 中连接到处理器（如 MicroBlaze 或 Zynq PS）
connect_bd_intf_net [get_bd_intf_pins microblaze_0/M_AXI_DP] [get_bd_intf_pins calc_socs_hls_1/s_axi_control]

# 或使用 SmartConnect 互联
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M00_AXI] [get_bd_intf_pins calc_socs_hls_1/s_axi_control]
```

---

### 2.3 AXI4-Lite Slave 地址接口（s_axi_control_r）

**用途**: 传递 DDR 数据缓冲区的基地址（pointer 参数）

| 端口名 | 方向 | 位宽 | AXI信号类型 | 连线目标 |
|--------|------|------|------------|----------|
| `s_axi_control_r_AWVALID` | Input | 1 | Write Address Valid | 处理器 AXI-Lite Master |
| `s_axi_control_r_AWREADY` | Output | 1 | Write Address Ready | 处理器 AXI-Lite Master |
| `s_axi_control_r_AWADDR` | Input | 7 | Write Address | 处理器 AXI-Lite Master |
| `s_axi_control_r_WVALID` | Input | 1 | Write Data Valid | 处理器 AXI-Lite Master |
| `s_axi_control_r_WREADY` | Output | 1 | Write Data Ready | 处理器 AXI-Lite Master |
| `s_axi_control_r_WDATA` | Input | 32 | Write Data | 处理器 AXI-Lite Master |
| `s_axi_control_r_WSTRB` | Input | 4 | Write Strobe | 处理器 AXI-Lite Master |
| `s_axi_control_r_ARVALID` | Input | 1 | Read Address Valid | 处理器 AXI-Lite Master |
| `s_axi_control_r_ARREADY` | Output | 1 | Read Address Ready | 处理器 AXI-Lite Master |
| `s_axi_control_r_ARADDR` | Input | 7 | Read Address | 处理器 AXI-Lite Master |
| `s_axi_control_r_RVALID` | Output | 1 | Read Data Valid | 处理器 AXI-Lite Master |
| `s_axi_control_r_RREADY` | Input | 1 | Read Data Ready | 处理器 AXI-Lite Master |
| `s_axi_control_r_RDATA` | Output | 32 | Read Data | 处理器 AXI-Lite Master |
| `s_axi_control_r_RRESP` | Output | 2 | Read Response | 处理器 AXI-Lite Master |
| `s_axi_control_r_BVALID` | Output | 1 | Write Response Valid | 处理器 AXI-Lite Master |
| `s_axi_control_r_BREADY` | Input | 1 | Write Response Ready | 处理器 AXI-Lite Master |
| `s_axi_control_r_BRESP` | Output | 2 | Write Response | 处理器 AXI-Lite Master |

**地址寄存器映射**（7位地址 = 128字节空间）：

| 地址偏移 | 寄存器名称 | 位宽 | 功能 | 数据内容 |
|----------|-----------|------|------|----------|
| `0x00` | `mskf_r` | 32 | Mask spectrum real pointer | **DDR基地址**（512×512 float = 1MB） |
| `0x04` | `mskf_i` | 32 | Mask spectrum imag pointer | **DDR基地址**（512×512 float = 1MB） |
| `0x08` | `scales` | 32 | Eigenvalue scales pointer | **DDR基地址**（10 float = 40B） |
| `0x0C` | `krn_r` | 32 | SOCS kernels real pointer | **DDR基地址**（10×9×9 float = 810B） |
| `0x10` | `krn_i` | 32 | SOCS kernels imag pointer | **DDR基地址**（10×9×9 float = 810B） |
| `0x14` | `output` | 32 | Output buffer pointer | **DDR基地址**（17×17 float = 289B） |

**连线建议**：
```tcl
# 与 s_axi_control 类似，连接到处理器 AXI-Lite Master
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M01_AXI] [get_bd_intf_pins calc_socs_hls_1/s_axi_control_r]
```

---

### 2.4 AXI4-Master 内存接口（m_axi_gmem[0-5]）

**用途**: 直接访问 DDR 内存，读取输入数据、写入输出结果

每个 `m_axi_gmem` 接口包含完整的 AXI4 信号集（ burst 传输支持）：

#### 通用信号列表（适用于 gmem0-gmem5）

| 信号组 | 端口名示例 | 方向 | 位宽 | 功能 |
|--------|-----------|------|------|------|
| **Write Address Channel** | `m_axi_gmemX_AWVALID` | Output | 1 | Write address valid |
| | `m_axi_gmemX_AWREADY` | Input | 1 | Write address ready |
| | `m_axi_gmemX_AWADDR` | Output | 64 | Write address (full DDR range) |
| | `m_axi_gmemX_AWID` | Output | 1 | Transaction ID |
| | `m_axi_gmemX_AWLEN` | Output | 8 | Burst length (0-255 beats) |
| | `m_axi_gmemX_AWSIZE` | Output | 3 | Burst size (2=4 bytes) |
| | `m_axi_gmemX_AWBURST` | Output | 2 | Burst type (01=INCR) |
| | `m_axi_gmemX_AWLOCK` | Output | 1 | Lock type |
| | `m_axi_gmemX_AWCACHE` | Output | 4 | Cache attributes |
| | `m_axi_gmemX_AWPROT` | Output | 3 | Protection type |
| | `m_axi_gmemX_AWQOS` | Output | 4 | QoS priority |
| | `m_axi_gmemX_AWREGION` | Output | 4 | Region identifier |
| | `m_axi_gmemX_AWUSER` | Output | 1 | User-defined signals |
| **Write Data Channel** | `m_axi_gmemX_WVALID` | Output | 1 | Write data valid |
| | `m_axi_gmemX_WREADY` | Input | 1 | Write data ready |
| | `m_axi_gmemX_WDATA` | Output | 32 | Write data payload |
| | `m_axi_gmemX_WSTRB` | Output | 4 | Write strobe (byte mask) |
| | `m_axi_gmemX_WLAST` | Output | 1 | Last beat of burst |
| | `m_axi_gmemX_WID` | Output | 1 | Transaction ID |
| | `m_axi_gmemX_WUSER` | Output | 1 | User-defined signals |
| **Write Response Channel** | `m_axi_gmemX_BVALID` | Input | 1 | Write response valid |
| | `m_axi_gmemX_BREADY` | Output | 1 | Write response ready |
| | `m_axi_gmemX_BRESP` | Input | 2 | Write response (OKAY/EXOKAY/SLVERR/DECERR) |
| | `m_axi_gmemX_BID` | Input | 1 | Transaction ID |
| | `m_axi_gmemX_BUSER` | Input | 1 | User-defined signals |
| **Read Address Channel** | `m_axi_gmemX_ARVALID` | Output | 1 | Read address valid |
| | `m_axi_gmemX_ARREADY` | Input | 1 | Read address ready |
| | `m_axi_gmemX_ARADDR` | Output | 64 | Read address |
| | `m_axi_gmemX_ARID` | Output | 1 | Transaction ID |
| | `m_axi_gmemX_ARLEN` | Output | 8 | Burst length |
| | `m_axi_gmemX_ARSIZE` | Output | 3 | Burst size |
| | `m_axi_gmemX_ARBURST` | Output | 2 | Burst type |
| | `m_axi_gmemX_ARLOCK` | Output | 1 | Lock type |
| | `m_axi_gmemX_ARCACHE` | Output | 4 | Cache attributes |
| | `m_axi_gmemX_ARPROT` | Output | 3 | Protection type |
| | `m_axi_gmemX_ARQOS` | Output | 4 | QoS priority |
| | `m_axi_gmemX_ARREGION` | Output | 4 | Region identifier |
| | `m_axi_gmemX_ARUSER` | Output | 1 | User-defined signals |
| **Read Data Channel** | `m_axi_gmemX_RVALID` | Input | 1 | Read data valid |
| | `m_axi_gmemX_RREADY` | Output | 1 | Read data ready |
| | `m_axi_gmemX_RDATA` | Input | 32 | Read data payload |
| | `m_axi_gmemX_RLAST` | Input | 1 | Last beat of burst |
| | `m_axi_gmemX_RID` | Input | 1 | Transaction ID |
| | `m_axi_gmemX_RUSER` | Input | 1 | User-defined signals |
| | `m_axi_gmemX_RRESP` | Input | 2 | Read response |

#### 各 gmem 接口的具体用途

| 接口名 | 对应参数 | 数据大小 | 访问模式 | 连线建议 |
|--------|----------|----------|----------|----------|
| `m_axi_gmem0` | `mskf_r` | 512×512×4B = **1 MB** | 只读 | 连接到 DDR 控制器的 AXI Interconnect |
| `m_axi_gmem1` | `mskf_i` | 512×512×4B = **1 MB** | 只读 | 连接到 DDR 控制器的 AXI Interconnect |
| `m_axi_gmem2` | `scales` | 10×4B = **40 B** | 只读 | 连接到 DDR 控制器的 AXI Interconnect |
| `m_axi_gmem3` | `krn_r` | 10×9×9×4B = **810 B** | 只读 | 连接到 DDR 控制器的 AXI Interconnect |
| `m_axi_gmem4` | `krn_i` | 10×9×9×4B = **810 B** | 只读 | 连接到 DDR 控制器的 AXI Interconnect |
| `m_axi_gmem5` | `output` | 17×17×4B = **289 B** | 只写 | 连接到 DDR 控制器的 AXI Interconnect |

**连线建议**：
```tcl
# 在 Vivado Block Design 中，推荐使用 AXI SmartConnect 或 Interconnect
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 axi_smc
set_property -dict [list CONFIG.NUM_SI {6}] [get_bd_cells axi_smc]

# 连接 6 个 Master 接口
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem0] [get_bd_intf_pins axi_smc/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem1] [get_bd_intf_pins axi_smc/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem2] [get_bd_intf_pins axi_smc/S02_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem3] [get_bd_intf_pins axi_smc/S03_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem4] [get_bd_intf_pins axi_smc/S04_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem5] [get_bd_intf_pins axi_smc/S05_AXI]

# 连接到 DDR 控制器（如 MIG 或 Zynq PS）
connect_bd_intf_net [get_bd_intf_pins axi_smc/M00_AXI] [get_bd_intf_pins mig_7series_0/S_AXI]
```

---

### 2.5 中断端口

| 端口名 | 方向 | 位宽 | 功能 | 连线目标 |
|--------|------|------|------|----------|
| `interrupt` | Output | 1 | IP 完成中断（高电平有效） | 处理器中断控制器（如 MicroBlaze Xilinx Interrupt Controller 或 Zynq PS GIC） |

**连线建议**：
```tcl
# 连接到处理器的中断控制器
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_0
connect_bd_net [get_bd_pins calc_socs_hls_1/interrupt] [get_bd_pins xlconcat_0/In0]
connect_bd_net [get_bd_pins xlconcat_0/dout] [get_bd_pins microblaze_0/INTERRUPT]
```

---

## 3. Vivado Block Design 快速集成模板

以下是一个完整的 Vivado Block Design TCL 脚本模板，用于集成 `calc_socs_hls` IP：

```tcl
# ============================================================
# Vivado Block Design Integration Script for calc_socs_hls IP
# ============================================================

# 1. 创建 Block Design
create_bd_design "socs_system"

# 2. 添加 IP（假设已导入到 IP Catalog）
create_bd_cell -type ip -vlnv xilinx.com:hls:calc_socs_hls:1.0 calc_socs_hls_1

# 3. 添加处理器系统（示例：MicroBlaze）
create_bd_cell -type ip -vlnv xilinx.com:ip:microblaze:11.0 microblaze_0
apply_bd_automation -rule xilinx.com:bd_rule:microblaze -config {local_mem "128KB" ecc "None" cache "None" debug_module "DebugOnly" axi_periph "Enabled" interrupts "Enabled" }  [get_bd_cells microblaze_0]

# 4. 添加 AXI SmartConnect（用于 gmem 接口）
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 axi_smc_gmem
set_property -dict [list CONFIG.NUM_SI {6}] [get_bd_cells axi_smc_gmem]

# 5. 连接 gmem 接口到 DDR
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem0] [get_bd_intf_pins axi_smc_gmem/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem1] [get_bd_intf_pins axi_smc_gmem/S01_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem2] [get_bd_intf_pins axi_smc_gmem/S02_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem3] [get_bd_intf_pins axi_smc_gmem/S03_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem4] [get_bd_intf_pins axi_smc_gmem/S04_AXI]
connect_bd_intf_net [get_bd_intf_pins calc_socs_hls_1/m_axi_gmem5] [get_bd_intf_pins axi_smc_gmem/S05_AXI]

# 连接到 DDR 控制器（根据实际平台调整）
# 例如：Zynq PS 的 M_AXI_GP0 → axi_smc_gmem → DDR
# 或者：MIG DDR 控制器的 S_AXI

# 6. 连接控制接口到处理器
connect_bd_intf_net [get_bd_intf_pins microblaze_0/M_AXI_DP] [get_bd_intf_pins calc_socs_hls_1/s_axi_control]
connect_bd_intf_net [get_bd_intf_pins microblaze_0/M_AXI_DP] [get_bd_intf_pins calc_socs_hls_1/s_axi_control_r]

# 7. 连接中断
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 interrupt_concat
connect_bd_net [get_bd_pins calc_socs_hls_1/interrupt] [get_bd_pins interrupt_concat/In0]
connect_bd_net [get_bd_pins interrupt_concat/dout] [get_bd_pins microblaze_0/INTERRUPT]

# 8. 连接时钟和复位
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins calc_socs_hls_1/ap_clk]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins calc_socs_hls_1/ap_rst_n]

# 9. 地址分配（自动分配）
assign_bd_address

# 10. 验证并保存
validate_bd_design
save_bd_design

# 11. 生成 HDL Wrapper
make_wrapper -files [get_files socs_system.bd] -top
add_files -norecurse [get_files socs_system_wrapper.v]
```

---

## 4. 软件驱动使用示例

### 4.1 地址分配（自动分配后）

假设 Vivado 自动分配的地址如下（实际需查看 Address Editor）：

| IP/接口 | 基地址 | 地址范围 |
|---------|--------|----------|
| `s_axi_control` | `0x40000000` | 16B |
| `s_axi_control_r` | `0x40010000` | 128B |
| DDR Memory（数据缓冲区） | `0x80000000` | 动态分配 |

### 4.2 C 语言驱动示例（基于 xilinx_hls_calc_socs_hls.h）

```c
#include "xilinx_com_hls_calc_socs_hls_1_0/drivers/xilinx_com_hls_calc_socs_hls_1_0/src/xilinx_com_hls_calc_socs_hls.h"

// 定义基地址
#define SOC_HLS_CTRL_BASE   0x40000000
#define SOC_HLS_PARAM_BASE  0x40010000
#define DDR_BASE            0x80000000

// 定义数据缓冲区地址（在 DDR 中）
#define MSKF_R_ADDR   (DDR_BASE + 0x000000)  // 1MB
#define MSKF_I_ADDR   (DDR_BASE + 0x100000)  // 1MB
#define SCALES_ADDR   (DDR_BASE + 0x200000)  // 40B
#define KRN_R_ADDR    (DDR_BASE + 0x200040)  // 810B
#define KRN_I_ADDR    (DDR_BASE + 0x200650)  // 810B
#define OUTPUT_ADDR   (DDR_BASE + 0x200EA0)  // 289B

int main() {
    // 1. 初始化驱动
    XCalc_socs_hls_Config *config = XCalc_socs_hls_LookupConfig(0);
    XCalc_socs_hls instance;
    XCalc_socs_hls_CfgInitialize(&instance, config);
    
    // 2. 设置数据缓冲区地址（写入 s_axi_control_r 寄存器）
    XCalc_socs_hls_Set_mskf_r(&instance, MSKF_R_ADDR);
    XCalc_socs_hls_Set_mskf_i(&instance, MSKF_I_ADDR);
    XCalc_socs_hls_Set_scales(&instance, SCALES_ADDR);
    XCalc_socs_hls_Set_krn_r(&instance, KRN_R_ADDR);
    XCalc_socs_hls_Set_krn_i(&instance, KRN_I_ADDR);
    XCalc_socs_hls_Set_output(&instance, OUTPUT_ADDR);
    
    // 3. 启动 IP（写 ap_start = 1）
    XCalc_socs_hls_Start(&instance);
    
    // 4. 等待完成（轮询 ap_done）
    while (!XCalc_socs_hls_IsDone(&instance)) {
        // 可添加超时保护
    }
    
    // 5. 读取结果（从 OUTPUT_ADDR）
    float *result = (float *)OUTPUT_ADDR;
    printf("Output first value: %f\n", result[0]);
    
    return 0;
}
```

---

## 5. JTAG-to-AXI 硬件调试流程

如果需要通过 Vivado Hardware Manager 直接测试 IP（无处理器），可使用以下 TCL 命令：

```tcl
# 1. 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. Reset JTAG-to-AXI Master
reset_hw_axi [get_hw_axis hw_axi_1]

# 3. 配置数据地址（写 s_axi_control_r 寄存器）
# 假设 DDR 基地址为 0x80000000，数据已预加载

# 写 mskf_r 地址（offset 0x00）
create_hw_axi_txn wr_mskf_r [get_hw_axis hw_axi_1] \
  -address 0x40010000 -data 0x80000000 -len 1 -type write
run_hw_axi_txn wr_mskf_r

# 写 mskf_i 地址（offset 0x04）
create_hw_axi_txn wr_mskf_i [get_hw_axis hw_axi_1] \
  -address 0x40010004 -data 0x80100000 -len 1 -type write
run_hw_axi_txn wr_mskf_i

# ... 其他地址配置类似 ...

# 4. 启动 IP（写 ap_start = 1）
create_hw_axi_txn start_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000001 -len 1 -type write
run_hw_axi_txn start_txn

# 5. 等待完成（轮询 ap_done，bit[1]）
# 建议封装成轮询 proc，添加超时保护
proc wait_done {} {
    set timeout 0
    while {$timeout < 100000} {
        create_hw_axi_txn rd_status [get_hw_axis hw_axi_1] \
          -address 0x40000000 -len 1 -type read
        run_hw_axi_txn rd_status
        set status [get_property DATA [get_hw_axi_txn rd_status]]
        if {[expr {$status & 0x2}] != 0} {
            puts "IP DONE!"
            return
        }
        incr timeout
    }
    puts "ERROR: Timeout waiting for ap_done"
}

wait_done

# 6. 读取结果（从 DDR 输出缓冲区）
create_hw_axi_txn rd_output [get_hw_axis hw_axi_1] \
  -address 0x80000EA0 -len 17 -type read
run_hw_axi_txn rd_output
```

---

## 6. 常见问题与解决方案

### Q1: 6 个 AXI-Master 接口占用资源过多，能否合并？

**方案**: 在 HLS 源码中修改 bundle 参数，将多个参数合并到同一 AXI-Master：

```cpp
// 原代码（6 个独立接口）
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem1
...

// 修改后（合并到同一接口）
#pragma HLS INTERFACE m_axi port=mskf_r offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=mskf_i offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=krn_r offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=krn_i offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem0
```

**影响**: 
- 优点：减少 AXI Interconnect 复杂度，节省资源
- 缺点：带宽降低，多个参数串行传输

### Q2: 如何优化 DDR 访问带宽？

**建议**:
1. 使用 AXI SmartConnect（优于传统 Interconnect）
2. 配置 `max_read_burst_length` 和 `max_write_burst_length`（在 HLS config 中）
3. 启用 Data Cache（如果处理器支持）

### Q3: 时钟频率达不到 273.97 MHz怎么办？

**方案**:
1. 使用 PLL/MMCM 生成精确时钟
2. 如果目标频率较低（如 100 MHz），需重新综合（修改 `clock` 参数）
3. 检查时序约束文件（XDC）是否正确

---

## 7. 参考文档

- **HLS 源码**: `source/SOCS_HLS/src/socs_hls.cpp`
- **IP RTL**: `source/SOCS_HLS/socs_full_csynth_v10/hls/impl/verilog/calc_socs_hls.v`
- **IP Catalog XML**: `source/SOCS_HLS/socs_full_csynth_v10/hls/impl/ip/component.xml`
- **驱动模板**: `source/SOCS_HLS/socs_full_csynth_v10/hls/impl/ip/drivers/xilinx_com_hls_calc_socs_hls_1_0/src/`
- **Vivado TCL 参考**: `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl`
- **HLS-to-FPGA Workflow**: `reference/参考文档/FPGA-Litho HLS-to-FPGA Workflow & TCL Verification Guide.md`

---

**文档版本**: v1.0 (2026-04-08)
**生成工具**: Vitis HLS 2025.2
**维护者**: FPGA-Litho Team