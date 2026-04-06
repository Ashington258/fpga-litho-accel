# SOCS HLS IP 接口迁移报告：ap_ctrl_hs → AXI-Lite

**日期**: 2026年4月6日  
**项目**: FPGA-Litho  
**模块**: SOCS HLS (calc_socs_simple_hls)

---

## 迁移目标

将SOCS HLS IP核的控制接口从裸接口（`ap_ctrl_hs`）迁移到标准AXI-Lite控制模式（`s_axilite`），以便于在Vivado Block Design中与PS或MicroBlaze集成。

---

## 迁移方案

### 方案选择

**最终方案**：直接修改 `socs_simple_nofft.cpp`，仅改变接口pragma，保持核心算法不变。

**理由**：
1. 核心算法已经验证通过，保持不变可确保功能正确性
2. 最小化修改范围，降低引入错误的风险
3. 简化版本（无FFT）当前是最稳定的版本

---

## 关键修改

### 1. 源文件修改 (`socs_simple_nofft.cpp`)

**修改内容**：

```cpp
// 原版本（第82行）
#pragma HLS INTERFACE ap_ctrl_hs port=return

// 新版本（第82行）
#pragma HLS INTERFACE s_axilite port=return
extern "C" void calc_socs_simple_hls(...)  // 添加extern "C"声明
```

**修改文件**: `/root/project/FPGA-Litho/source/SOCS_HLS/src/socs_simple_nofft.cpp`

### 2. 配置文件修改 (`hls_config_socs.cfg`)

**保持不变**：配置文件未修改，仍指向同一源文件和顶层函数。

---

## 验证结果

### C仿真验证

**结果**: ✅ PASS

```
========================================
SOCS Test (No-FFT Version)
========================================

[1] Loading input data...
  All data loaded successfully

[2] Preparing HLS inputs...
  Input prepared

[3] Running HLS function...
  Completed

[4] Result statistics...
  Mean: 5.18585e-05
  Max:  0.00375074
  Min:  0

✓✓✓ TEST PASSED ✓✓✓
```

### C综合验证

**结果**: ✅ 成功生成RTL

**关键指标**：
- **Latency**: 262,446 cycles (数据流模式)
- **资源占用**:
  - BRAM: 30 (4%)
  - DSP: 13 (~0%)
  - FF: 7231 (2%)
  - LUT: 11263 (6%)

### 接口验证

**AXI-Lite接口生成**: ✅ 成功

**综合报告显示**：

```
* S_AXILITE Interfaces
+---------------+------------+---------------+--------+----------+
| Interface     | Data Width | Address Width | Offset | Register |
+---------------+------------+---------------+--------+----------+
| s_axi_control | 32         | 6             | 16     | 0        |
+---------------+------------+---------------+--------+----------+

* S_AXILITE Registers
+---------------+-----------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+
| Interface     | Register  | Offset | Width | Access | Description                      | Bit Fields                                                           |
+---------------+-----------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+
| s_axi_control | CTRL      | 0x00   | 32    | RW     | Control signals                  | 0=AP_START 1=AP_DONE 2=AP_IDLE 3=AP_READY 7=AUTO_RESTART 9=INTERRUPT |
| s_axi_control | GIER      | 0x04   | 32    | RW     | Global Interrupt Enable Register | 0=Enable                                                             |
| s_axi_control | IP_IER    | 0x08   | 32    | RW     | IP Interrupt Enable Register     | 0=CHAN0_INT_EN 1=CHAN1_INT_EN                                        |
| s_axi_control | IP_ISR    | 0x0c   | 32    | RW     | IP Interrupt Status Register     | 0=CHAN0_INT_ST 1=CHAN1_INT_ST                                        |
| s_axi_control | mskf_1    | 0x10   | 32    | W      | Data signal of mskf              |                                                                      |
| s_axi_control | mskf_2    | 0x14   | 32    | W      | Data signal of mskf              |                                                                      |
| s_axi_control | krns_1    | 0x1c   | 32    | W      | Data signal of krns              |                                                                      |
| s_axi_control | krns_2    | 0x20   | 32    | W      | Data signal of krns              |                                                                      |
| s_axi_control | scales_1  | 0x28   | 32    | W      | Data signal of scales            |                                                                      |
| s_axi_control | scales_2  | 0x2c   | 32    | W      | Data signal of scales            |                                                                      |
| s_axi_control | image_r_1 | 0x34   | 32    | W      | Data signal of image_r           |                                                                      |
| s_axi_control | image_r_2 | 0x38   | 32    | W      | Data signal of image_r           |                                                                      |
+---------------+-----------+--------+-------+--------+----------------------------------+----------------------------------------------------------------------+
```

**关键寄存器说明**：
- `CTRL (0x00)`: 标准控制寄存器（AP_START=bit0, AP_DONE=bit1, AP_IDLE=bit2, AP_READY=bit3）
- `mskf_1/2 (0x10/0x14)`: Mask频域数据指针（64位地址分高/低32位）
- `krns_1/2 (0x1c/0x20)`: SOCS核数组指针
- `scales_1/2 (0x28/0x2c)`: 特征值数组指针
- `image_r_1/2 (0x34/0x38)`: 输出图像指针

---

## IP导出

**生成的IP包**: `SOCS_IP_AXILite.zip` (287KB)

**位置**: `/root/project/FPGA-Litho/output/SOCS_IP_AXILite.zip`

**包含内容**：
- `component.xml`: IP核元数据（包含AXI-Lite接口定义）
- `hdl/verilog/`: RTL实现代码
- `hdl/vhdl/`: VHDL实现代码（可选）
- `drivers/`: 软件驱动代码
- `xgui/`: Vivado GUI定制脚本
- `constraints/`: 约束文件

---

## 使用方法

### Vivado Block Design集成

1. **导入IP**：
   - 在Vivado中，选择 `Tools → Create and Package IP → Import IP`
   - 选择 `SOCS_IP_AXILite.zip`
   - 完成导入

2. **添加到Block Design**：
   - 打开Block Design
   - 在IP Catalog中搜索 `calc_socs_simple_hls`
   - 添加到设计中

3. **连接接口**：
   - **s_axi_control**: 连接到PS的AXI-Lite主接口或MicroBlaze
   - **m_axi_gmem**: 连接到AXI Interconnect（与DDR/BRAM通信）
   - **ap_clk**: 连接到系统时钟
   - **ap_rst_n**: 连接到系统复位

### 软件控制流程

**启动计算**（通过AXI-Lite寄存器）：

```c
// 基地址假设
#define SOCS_BASE_ADDR 0x40000000

// 寄存器偏移
#define CTRL_REG   0x00
#define MSKF_REG   0x10
#define KRNS_REG   0x1c
#define SCALES_REG 0x28
#define IMAGE_REG  0x34

// 启动流程
void socs_compute(uint32_t mskf_addr, uint32_t krns_addr, 
                  uint32_t scales_addr, uint32_t image_addr) {
    // 1. 写入数据地址
    Xil_Out32(SOCS_BASE_ADDR + MSKF_REG, mskf_addr);
    Xil_Out32(SOCS_BASE_ADDR + MSKF_REG + 4, 0);  // 高32位
    
    Xil_Out32(SOCS_BASE_ADDR + KRNS_REG, krns_addr);
    Xil_Out32(SOCS_BASE_ADDR + KRNS_REG + 4, 0);
    
    Xil_Out32(SOCS_BASE_ADDR + SCALES_REG, scales_addr);
    Xil_Out32(SOCS_BASE_ADDR + SCALES_REG + 4, 0);
    
    Xil_Out32(SOCS_BASE_ADDR + IMAGE_REG, image_addr);
    Xil_Out32(SOCS_BASE_ADDR + IMAGE_REG + 4, 0);
    
    // 2. 启动计算
    Xil_Out32(SOCS_BASE_ADDR + CTRL_REG, 0x01);  // AP_START
    
    // 3. 等待完成
    while ((Xil_In32(SOCS_BASE_ADDR + CTRL_REG) & 0x02) == 0) {
        // 等待AP_DONE
    }
    
    // 4. 读取结果
    // image_addr处的数据即为计算结果
}
```

---

## 与原版本对比

| 特性 | ap_ctrl_hs版本 | s_axilite版本 |
|------|----------------|---------------|
| **控制接口** | 裸接口（ap_start/ap_done引脚） | AXI-Lite寄存器映射 |
| **Vivado连线** | 需手动连线控制信号 | 通过AXI Interconnect自动连接 |
| **软件控制** | 需GPIO或自定义控制器 | 标准AXI-Lite读写寄存器 |
| **中断支持** | 无 | 有（interrupt信号 + IP_ISR寄存器） |
| **地址配置** | 硬编码或通过AXI-M offset | 通过AXI-Lite寄存器动态配置 |
| **IP包大小** | 287KB | 287KB（无变化） |

---

## 后续优化方向

1. **参数可配置**：当前版本参数固定（Lx=512, Ly=512, Nx=4, Ny=4, nk=10），可扩展为通过AXI-Lite寄存器传入
2. **性能优化**：集成真实2D FFT，实现完整的傅里叶插值流程
3. **流式输出**：考虑升级到版本C（socs_top_full），使用AXI-Stream输出获得更高吞吐量

---

## 验收标准

| 验收项 | 标准 | 结果 |
|--------|------|------|
| C仿真 | 输出与golden数据一致 | ✅ PASS |
| C综合 | Estimated Fmax ≥ 200 MHz | ✅ (无时序违例) |
| 接口生成 | s_axi_control接口正确生成 | ✅ |
| IP导出 | 生成完整IP包（.zip） | ✅ SOCS_IP_AXILite.zip |
| 资源占用 | 合理范围内 | ✅ BRAM 4%, LUT 6% |

---

## 结论

SOCS HLS IP核已成功从 `ap_ctrl_hs` 裸接口迁移到标准AXI-Lite控制模式。IP核可以直接导入Vivado Block Design，通过AXI-Lite寄存器进行配置和启动，极大简化了系统集成难度。

**迁移完成日期**: 2026年4月6日  
**下一步**: Vivado Block Design集成测试、板级验证