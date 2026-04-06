# SOCS HLS IP核 Vivado Block Design 接线方案

> **目标**: 通过JTAG注入参数和数据到BRAM，IP核读取后计算空中像，结果返回PC分析  
> **IP核**: `xilinx_com_hls_calc_socs_simple_hls_1_0`  
> **器件**: xcku3p-ffvb676-2-e  
> **创建日期**: 2026-04-06

---

## 一、IP核接口分析

### 1.1 接口类型

| 接口名称 | 类型 | 协议 | 用途 | 数据宽度 |
|---------|------|------|------|---------|
| **s_axi_control** | Slave | AXI4-Lite | 参数配置（指针地址） | 32-bit |
| **m_axi_gmem** | Master | AXI4 | 数据访问（读输入/写输出） | 64-bit |
| **ap_clk** | Input | Clock | 时钟输入 | 1-bit |
| **ap_rst_n** | Input | Reset | 异步复位（低有效） | 1-bit |
| **ap_start** | Input | Control | 启动信号 | 1-bit |
| **ap_done** | Output | Control | 完成信号 | 1-bit |
| **ap_idle** | Output | Control | 空闲状态 | 1-bit |
| **ap_ready** | Output | Control | 就绪信号 | 1-bit |

### 1.2 AXI-Lite控制寄存器映射

| 偏移地址 | 寄存器名 | 功能 | 数据类型 |
|---------|---------|------|---------|
| 0x10 | mskf_1 | Mask频域数据指针（低32位） | uint32 |
| 0x14 | mskf_2 | Mask频域数据指针（高32位） | uint32 |
| 0x1C | krns_1 | SOCS核数组指针（低32位） | uint32 |
| 0x20 | krns_2 | SOCS核数组指针（高32位） | uint32 |
| 0x28 | scales_1 | 特征值数组指针（低32位） | uint32 |
| 0x2C | scales_2 | 特征值数组指针（高32位） | uint32 |
| 0x34 | image_r_1 | 输出图像指针（低32位） | uint32 |
| 0x38 | image_r_2 | 输出图像指针（高32位） | uint32 |

---

## 二、Block Design 架构设计

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Vivado Block Design 架构                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────┐                    ┌────────────────────┐               │
│  │ JTAG-to-AXI   │                    │   Processing       │               │
│  │   Master      │                    │     System         │               │
│  ├───────────────┤                    ├────────────────────┤               │
│  │               │                    │                    │               │
│  │  USB-JTAG     │                    │  ┌──────────────┐  │               │
│  │     ↓         │                    │  │  AXI Inter-  │  │               │
│  │  hw_axi_1     │─────AXI-Master────→│  │   connect    │  │               │
│  │               │                    │  │  (S00_M_AXI) │  │               │
│  │               │                    │  └──────┬───────┘  │               │
│  │  功能：       │                    │         │          │               │
│  │  1. 配置参数  │                    │   ┌─────┴─────┬─────┴──────┐       │
│  │  2. 写入数据  │                    │   │           │            │       │
│  │  3. 读出结果  │                    │   │           │            │       │
│  │               │                    │   │           │            │       │
│  └───────────────┘                    │   │           │            │       │
│                                       │   ↓           ↓            ↓       │
│                                       │ ┌────────┐ ┌────────┐ ┌────────┐ │
│                                       │ │ M01_AXI│ │ M02_AXI│ │ M03_AXI│ │
│                                       │ └───┬────┘ └───┬────┘ └───┬────┘ │
│                                       │     │          │          │       │
│                                       │     ↓          ↓          ↓       │
│                                       │ ┌────────────────────────────┐   │
│                                       │ │   AXI BRAM Controller      │   │
│                                       │ │   (多个实例)               │   │
│                                       │ └────────────────────────────┘   │
│                                       │     │          │          │       │
│                                       │     ↓          ↓          ↓       │
│                                       │ ┌────────┐ ┌────────┐ ┌────────┐ │
│                                       │ │ BRAM   │ │ BRAM   │ │ BRAM   │ │
│                                       │ │ (mskf) │ │ (krns) │ │ (image)│ │
│                                       │ └────────┘ └────────┘ └────────┘ │
│                                       │                    │               │
│                                       │                    ↓               │
│                                       │            ┌──────────────┐       │
│                                       │            │  SOCS HLS IP │       │
│                                       │            │  m_axi_gmem  │←──────┤
│                                       │            │              │       │
│                                       │            │  s_axi_ctrl  │←──────┤
│                                       │            │              │       │
│                                       │            └──────────────┘       │
│                                       │                    │               │
│                                       │                    ↓               │
│                                       │              ap_done              │
│                                       └────────────────────┘               │
│                                                    │                       │
│                                                    ↓                       │
│                                            JTAG轮询读取                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 三、详细接线方案（JTAG-to-AXI 方案）

### 3.1 需添加的IP组件

| 序号 | IP名称 | 数量 | 功能 | 参数配置 |
|-----|--------|------|------|---------|
| 1 | **JTAG to AXI Master** | 1 | JTAG转AXI接口，用于配置和数据注入 | C_S_AXI_DATA_WIDTH=32 |
| 2 | **AXI Interconnect** | 2 | AXI总线交叉开关 | 1x3 (配置) + 3x1 (数据) |
| 3 | **AXI BRAM Controller** | 3 | BRAM控制器 | C_S_AXI_DATA_WIDTH=64 |
| 4 | **Block RAM Generator** | 3 | 存储输入数据和输出结果 | 根据尺寸配置 |
| 5 | **Processor System Reset** | 1 | 复位管理 | |
| 6 | **Clock Wizard** | 1 | 时钟生成（200MHz） | CLK_OUT1=5ns |

---

### 3.2 BRAM容量规划（默认配置：Lx=512, Ly=512, Nx=4, Ny=4, nk=10）

#### 数据尺寸计算：

```cpp
// 输入数据尺寸
int mskf_size = Lx * Ly * 8;           // 512×512×8 = 2MB (complex float32)
int krns_size = nk * (2*Nx+1) * (2*Ny+1) * 8;  // 10×9×9×8 = 6.4KB
int scales_size = nk * 4;               // 10×4 = 40B

// 输出数据尺寸
int image_size = Lx * Ly * 4;           // 512×512×4 = 1MB (float32)
```

#### BRAM实例分配：

| BRAM实例 | 存储内容 | 尅寸 | BRAM数量 | 参数配置 |
|---------|---------|------|---------|---------|
| **BRAM_mskf** | Mask频域数据 | 2MB | 277 BRAM（超BRAM容量，需用DDR） | **推荐改用DDR** |
| **BRAM_krns** | SOCS核数组 | 6.4KB | 1 BRAM | Memory Size: 8KB |
| **BRAM_scales** | 特征值数组 | 40B | 1 BRAM（与krns合并） | Memory Size: 64B |
| **BRAM_image** | 输出空中像 | 1MB | 139 BRAM（超BRAM容量，需用DDR） | **推荐改用DDR** |

**⚠️ 重要调整**：由于512×512尺寸超过BRAM容量（KU3P仅216个BRAM），推荐以下方案：

---

## 四、修订方案：DDR内存方案（推荐）

### 4.1 架构调整

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DDR内存方案架构                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────┐                    ┌────────────────────┐               │
│  │ JTAG-to-AXI   │                    │   Processing       │               │
│  │   Master      │                    │     System         │               │
│  ├───────────────┤                    ├────────────────────┤               │
│  │               │                    │                    │               │
│  │  hw_axi_1     │─────AXI-Master────→│  ┌──────────────┐  │               │
│  │               │                    │  │  AXI Inter-  │  │               │
│  │               │                    │  │   connect    │  │               │
│  └───────────────┘                    │  └──────┬───────┘  │               │
│                                       │         │          │               │
│                                       │   ┌─────┴─────┬─────┴──────┐       │
│                                       │   │           │            │       │
│                                       │   ↓           ↓            ↓       │
│                                       │ ┌────────┐ ┌────────┐ ┌────────┐ │
│                                       │ │ M01_AXI│ │ M02_AXI│ │ M03_AXI│ │
│                                       │ └───┬────┘ └───┬────┘ └───┬────┘ │
│                                       │     │          │          │       │
│                                       │     ↓          ↓          ↓       │
│                                       │ ┌────────────────────────────┐   │
│                                       │ │   AXI DDR Controller       │   │
│                                       │ │   (MIG)                    │   │
│                                       │ └────────────────────────────┘   │
│                                       │     │          │          │       │
│                                       │     ↓          ↓          ↓       │
│                                       │ ┌────────────────────────────┐   │
│                                       │ │   DDR4 SDRAM               │   │
│                                       │ │   (存储mskf, krns, image)  │   │
│                                       │ └────────────────────────────┘   │
│                                       │                    │               │
│                                       │                    ↓               │
│                                       │            ┌──────────────┐       │
│                                       │            │  SOCS HLS IP │       │
│                                       │            │              │       │
│                                       │            └──────────────┘       │
│                                       │                    │               │
│                                       │                    ↓               │
│                                       │              ap_done              │
│                                       └────────────────────┘               │
│                                                    │                       │
│                                                    ↓                       │
│                                            JTAG轮询读取                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 DDR方案IP组件

| 序号 | IP名称 | 数量 | 功能 | 参数配置 |
|-----|--------|------|------|---------|
| 1 | **JTAG to AXI Master** | 1 | JTAG转AXI接口 | C_S_AXI_DATA_WIDTH=32 |
| 2 | **AXI Interconnect** | 2 | 总线交叉开关 | 1xN (配置) + Nx1 (数据) |
| 3 | **MIG (DDR4 SDRAM Controller)** | 1 | DDR控制器 | 根据板卡配置 |
| 4 | **AXI BRAM Controller** | 1 | 小数据存储（krns+scales） | C_S_AXI_DATA_WIDTH=64 |
| 5 | **Block RAM Generator** | 1 | 存储krns+scales | 8KB |
| 6 | **Processor System Reset** | 1 | 复位管理 | |
| 7 | **Clock Wizard** | 1 | 时钟生成 | 200MHz + DDR时钟 |

---

## 五、详细接线步骤（DDR方案）

### 5.1 创建Block Design

#### 步骤1：添加基础IP

```tcl
# 创建Block Design
create_bd_design "socs_system"

# 添加JTAG to AXI Master
create_bd_cell -type ip -vlnv xilinx.com:ip:jtag_axi_master:1.0 jtag_axi_master_0

# 添加AXI Interconnect (配置总线)
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0
set_property -dict [list CONFIG.NUM_MI {3}] [get_bd_cells axi_interconnect_0]

# 添加AXI Interconnect (数据总线)
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_1
set_property -dict [list CONFIG.NUM_MI {1}] [get_bd_cells axi_interconnect_1]

# 添加MIG DDR Controller（根据板卡型号选择）
create_bd_cell -type ip -vlnv xilinx.com:ip:mig_7series:4.2 mig_7series_0

# 添加AXI BRAM Controller（小数据）
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_0
set_property -dict [list CONFIG.C_S_AXI_DATA_WIDTH {64}] [get_bd_cells axi_bram_ctrl_0]

# 添加Block RAM Generator
create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.4 blk_mem_gen_0
set_property -dict [list CONFIG.Memory_Type {Single_Port_RAM} CONFIG.Memory_Size {8KB}] [get_bd_cells blk_mem_gen_0]

# 添加SOCS HLS IP核
create_bd_cell -type ip -vlnv xilinx.com:hls:calc_socs_simple_hls:1.0 calc_socs_simple_hls_0

# 添加Clock Wizard
create_bd_cell -type ip -vlnv xilinx.com:ip:clk_wiz:6.0 clk_wiz_0
set_property -dict [list CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {200}] [get_bd_cells clk_wiz_0]

# 添加Processor System Reset
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0
```

#### 步骤2：连接时钟和复位

```tcl
# 连接时钟
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins calc_socs_simple_hls_0/ap_clk]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_interconnect_0/M00_ACLK]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_interconnect_0/M01_ACLK]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_interconnect_0/M02_ACLK]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins axi_interconnect_0/S00_ACLK]
connect_bd_net [get_bd_pins clk_wiz_0/clk_out1] [get_bd_pins jtag_axi_master_0/m_axi_aclk]

# 连接复位
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins calc_socs_simple_hls_0/ap_rst_n]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins axi_interconnect_0/M00_ARESETN]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins axi_interconnect_0/S00_ARESETN]
connect_bd_net [get_bd_pins clk_wiz_0/locked] [get_bd_pins proc_sys_reset_0/dcm_locked]
```

#### 步骤3：连接AXI总线（配置通道）

```tcl
# JTAG Master -> AXI Interconnect (配置)
connect_bd_intf_net [get_bd_intf_pins jtag_axi_master_0/M_AXI] [get_bd_intf_pins axi_interconnect_0/S00_AXI]

# AXI Interconnect -> SOCS控制接口
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M00_AXI] [get_bd_intf_pins calc_socs_simple_hls_0/s_axi_control]

# AXI Interconnect -> MIG DDR Controller（配置端口）
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M01_AXI] [get_bd_intf_pins mig_7series_0/S_AXI]

# AXI Interconnect -> BRAM Controller
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M02_AXI] [get_bd_intf_pins axi_bram_ctrl_0/S_AXI]

# BRAM Controller -> BRAM
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_0/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_0/BRAM_PORTA]
```

#### 步骤4：连接AXI总线（数据通道）

```tcl
# SOCS IP -> AXI Interconnect (数据访问)
connect_bd_intf_net [get_bd_intf_pins calc_socs_simple_hls_0/m_axi_gmem] [get_bd_intf_pins axi_interconnect_1/S00_AXI]

# AXI Interconnect -> MIG DDR Controller（数据端口）
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_1/M00_AXI] [get_bd_intf_pins mig_7series_0/S_AXI]
```

#### 步骤5：连接ap_ctrl信号

```tcl
# ap_ctrl信号需要通过GPIO或直接连接JTAG Master
# 方案A：直接连接到JTAG Master的GPIO端口
# 方案B：通过AXI-GPIO IP核控制

# 推荐方案B：添加AXI-GPIO
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0
set_property -dict [list CONFIG.C_ALL_INPUTS {1} CONFIG.C_GPIO_WIDTH {4}] [get_bd_cells axi_gpio_0]

# 连接GPIO
connect_bd_net [get_bd_pins calc_socs_simple_hls_0/ap_done] [get_bd_pins axi_gpio_0/gpio_io_i[0]]
connect_bd_net [get_bd_pins calc_socs_simple_hls_0/ap_idle] [get_bd_pins axi_gpio_0/gpio_io_i[1]]
connect_bd_net [get_bd_pins calc_socs_simple_hls_0/ap_ready] [get_bd_pins axi_gpio_0/gpio_io_i[2]]

# 连接GPIO到配置总线
connect_bd_intf_net [get_bd_intf_pins axi_interconnect_0/M03_AXI] [get_bd_intf_pins axi_gpio_0/S_AXI]
```

---

## 六、内存地址分配方案

### 6.1 DDR地址空间规划

| 数据块 | DDR基地址 | 大小 | 用途 | 访问方式 |
|--------|----------|------|------|---------|
| **mskf** | 0x0000_0000 | 2MB | Mask频域输入 | JTAG写入 → IP读取 |
| **krns** | 0x0020_0000 | 8KB | SOCS核数组 | JTAG写入 → IP读取 |
| **scales** | 0x0020_2000 | 64B | 特征值数组 | JTAG写入 → IP读取 |
| **image** | 0x0030_0000 | 1MB | 输出空中像 | IP写入 → JTAG读取 |

### 6.2 指针配置示例（通过JTAG）

```tcl
# 配置SOCS IP核的AXI-Lite寄存器
# 写入数据指针地址

# 1. mskf指针（0x0000_0000）
create_hw_axi_txn wr_mskf_1 [get_hw_axis hw_axi_1] -address 0x40000010 -data 0x00000000 -len 1 -type write
run_hw_axi_txn wr_mskf_1
create_hw_axi_txn wr_mskf_2 [get_hw_axis hw_axi_1] -address 0x40000014 -data 0x00000000 -len 1 -type write
run_hw_axi_txn wr_mskf_2

# 2. krns指针（0x0020_0000）
create_hw_axi_txn wr_krns_1 [get_hw_axis hw_axi_1] -address 0x4000001C -data 0x00200000 -len 1 -type write
run_hw_axi_txn wr_krns_1
create_hw_axi_txn wr_krns_2 [get_hw_axis hw_axi_1] -address 0x40000020 -data 0x00000000 -len 1 -type write
run_hw_axi_txn wr_krns_2

# 3. scales指针（0x0020_2000）
create_hw_axi_txn wr_scales_1 [get_hw_axis hw_axi_1] -address 0x40000028 -data 0x00202000 -len 1 -type write
run_hw_axi_txn wr_scales_1
create_hw_axi_txn wr_scales_2 [get_hw_axis hw_axi_1] -address 0x4000002C -data 0x00000000 -len 1 -type write
run_hw_axi_txn wr_scales_2

# 4. image指针（0x0030_0000）
create_hw_axi_txn wr_image_1 [get_hw_axis hw_axi_1] -address 0x40000034 -data 0x00300000 -len 1 -type write
run_hw_axi_txn wr_image_1
create_hw_axi_txn wr_image_2 [get_hw_axis hw_axi_1] -address 0x40000038 -data 0x00000000 -len 1 -type write
run_hw_axi_txn wr_image_2
```

---

## 七、完整JTAG操作流程

### 7.1 操作流程图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    JTAG操作流程                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Step 1: 系统初始化                                                          │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 1. connect_hw_server                                     │               │
│  │ 2. open_hw_target                                        │               │
│  │ 3. program_hw_devices (加载Bitstream)                   │               │
│  │ 4. reset_hw_axi (复位JTAG Master)                       │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 2: 数据注入（DDR）                                                     │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 1. 写入 mskf 数据到 DDR @ 0x0000_0000                    │               │
│  │    - 源文件: verification/mskf_r.bin + mskf_i.bin       │               │
│  │    - 大小: 512×512×8 = 2MB                               │               │
│  │                                                          │               │
│  │ 2. 写入 krns 数据到 DDR @ 0x0020_0000                    │               │
│  │    - 源文件: verification/kernels/*.bin                  │               │
│  │    - 大小: nk×(2Nx+1)×(2Ny+1)×8 ≈ 6.4KB                 │               │
│  │                                                          │               │
│  │ 3. 写入 scales 数据到 DDR @ 0x0020_2000                  │               │
│  │    - 源文件: verification/scales.bin                     │               │
│  │    - 大小: nk×4 ≈ 40B                                    │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 3: 配置IP参数                                                          │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 写入 AXI-Lite 寄存器（基地址：0x40000000）                │               │
│  │                                                          │               │
│  │ 0x10: mskf_ptr = 0x0000_0000                             │               │
│  │ 0x1C: krns_ptr = 0x0020_0000                             │               │
│  │ 0x28: scales_ptr = 0x0020_2000                           │               │
│  │ 0x34: image_ptr = 0x0030_0000                            │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 4: 启动计算                                                            │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 方案A：写 ap_start寄存器（如果HLS生成了）                │               │
│  │   - 写入 0x40000000 = 0x00000001                         │               │
│  │                                                          │               │
│  │ 方案B：通过GPIO触发（推荐）                              │               │
│  │   - 通过AXI-GPIO写入 ap_start=1                         │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 5: 等待完成                                                            │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 轮询 ap_done 状态（通过GPIO读取）                        │               │
│  │                                                          │               │
│  │ while (gpio_value[0] != 1) {                             │               │
│  │     read_gpio();                                         │               │
│  │     sleep(1ms);                                          │               │
│  │ }                                                        │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 6: 读出结果                                                            │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 从 DDR @ 0x0030_0000 读出空中像                          │               │
│  │                                                          │               │
│  │ create_hw_axi_txn rd_image [get_hw_axis hw_axi_1] \      │               │
│  │     -address 0x00300000 -len 262144 -type read           │               │
│  │                                                          │               │
│  │ 将数据保存为 image.bin（512×512 float32）               │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                    ↓                                        │
│                                                                             │
│  Step 7: 结果分析                                                            │
│  ┌──────────────────────────────────────────────────────────┐               │
│  │ 1. 比较输出与 Golden 数据 (verification/image.bin)       │               │
│  │ 2. 计算 RMSE、PSNR、SSIM                                 │               │
│  │ 3. 可视化（生成PNG图像）                                 │               │
│  └──────────────────────────────────────────────────────────┘               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 八、TCL脚本模板（完整版）

### 8.1 数据注入脚本

```tcl
# ==============================================================================
# SOCS系统JTAG测试脚本（DDR方案）
# ==============================================================================

# 1. 系统初始化
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]
reset_hw_axi [get_hw_axis hw_axi_1]

# 2. 写入mskf数据（从verification/mskf_r.bin + mskf_i.bin）
# 注意：需要先转换bin文件为AXI数据格式
# TODO: 实现bin2axi工具，将float32转换为AXI burst数据

# 示例：写入前4个测试数据
create_hw_axi_txn wr_mskf_test [get_hw_axis hw_axi_1] \
    -address 0x00000000 \
    -data {0x3F800000 0x00000000 0x3F800000 0x00000000} \
    -len 4 -type write
run_hw_axi_txn wr_mskf_test

# 3. 写入krns数据（从verification/kernels/*.bin）
# TODO: 批量写入SOCS核数据

# 4. 写入scales数据（从verification/scales.bin）
create_hw_axi_txn wr_scales [get_hw_axis hw_axi_1] \
    -address 0x00202000 \
    -data {0x3F800000 0x3F000000 0x3E800000} \
    -len 3 -type write
run_hw_axi_txn wr_scales

# 5. 配置IP指针（AXI-Lite）
# 假设SOCS IP基地址为0x40000000
create_hw_axi_txn cfg_mskf [get_hw_axis hw_axi_1] \
    -address 0x40000010 -data 0x00000000 -len 1 -type write
run_hw_axi_txn cfg_mskf

create_hw_axi_txn cfg_krns [get_hw_axis hw_axi_1] \
    -address 0x4000001C -data 0x00200000 -len 1 -type write
run_hw_axi_txn cfg_krns

create_hw_axi_txn cfg_scales [get_hw_axis hw_axi_1] \
    -address 0x40000028 -data 0x00202000 -len 1 -type write
run_hw_axi_txn cfg_scales

create_hw_axi_txn cfg_image [get_hw_axis hw_axi_1] \
    -address 0x40000034 -data 0x00300000 -len 1 -type write
run_hw_axi_txn cfg_image

# 6. 启动IP（假设通过GPIO）
# TODO: 需要确认ap_start的触发方式

# 7. 等待完成（轮询ap_done）
# TODO: 实现轮询逻辑

# 8. 读出结果
create_hw_axi_txn rd_image [get_hw_axis hw_axi_1] \
    -address 0x00300000 -len 128 -type read
run_hw_axi_txn rd_image

# 9. 关闭连接
close_hw_target
disconnect_hw_server
```

---

## 九、注意事项与建议

### 9.1 关键问题

| 问题 | 现状 | 解决方案 |
|------|------|---------|
| **ap_start触发** | HLS生成的IP无ap_start寄存器（ap_ctrl_none协议） | 需添加AXI-GPIO或修改HLS为ap_ctrl_hs |
| **数据注入效率** | JTAG速度慢，2MB数据注入耗时长 | 推荐使用DMA或PCIe（未来优化） |
| **BRAM容量** | 512×512超BRAM容量 | 使用DDR或降低测试尺寸（128×128） |
| **HLS IP地址** | 需确认Block Design中的IP基地址 | Vivado Address Editor中分配 |

### 9.2 推荐优化方向

1. **修改HLS接口协议**：
   ```cpp
   // 从 ap_ctrl_none 改为 ap_ctrl_hs
   #pragma HLS INTERFACE ap_ctrl_hs port=return
   ```
   这样会在AXI-Lite中生成ap_start寄存器（地址0x00）

2. **使用小尺寸验证**：
   - 测试尺寸：128×128（占用BRAM约0.5MB，可用BRAM方案）
   - 生产尺寸：512×512（使用DDR方案）

3. **添加状态寄存器**：
   - 在HLS中添加status输出（计算进度、错误状态）
   - 通过AXI-Lite读取，方便调试

---

## 十、验证方案

### 10.1 验证流程

```bash
# 1. 生成测试数据（128×128小尺寸）
cd /root/project/FPGA-Litho/verification
python generate_small_test.py --size 128

# 2. Vivado综合实现
cd vivado_project
vivado -mode batch -source implement_socs.tcl

# 3. JTAG测试
vivado -mode batch -source jtag_test_socs.tcl

# 4. 结果对比
python verify_result.py --hls_output jtag_image.bin --golden image.bin
```

### 10.2 验收标准

| 验收项 | 标准 | 方法 |
|--------|------|------|
| Block Design连线 | 无错误/警告 | Vivado Validate |
| Bitstream生成 | 成功 |vivado implement |
| JTAG数据注入 | 写入成功 | TCL日志 |
| 计算启动 | ap_start触发成功 | GPIO监控 |
| 结果读取 | 读出512×512数据 | TCL日志 |
| 数值精度 | RMSE < 1e-5 | Python对比 |

---

**文档版本**: v1.0  
**创建日期**: 2026-04-06  
**适用IP**: xilinx_com_hls_calc_socs_simple_hls_1_0  
**下一步**: Block Design创建、JTAG脚本实现、小尺寸验证