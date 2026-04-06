# Host适配器 - 后期升级路径

**当前状态**: Phase 0-4暂不实现Host端代码  
**后期阶段**: Phase 5+（PCIe升级）

## JTAG模式（当前，Phase 5）

### 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│ Vivado Block Design                                         │
│                                                             │
│  JTAG-to-AXI Master ────┬─── AXI BRAM Controller ── BRAM   │
│                         │                                   │
│                         └─── AXI Interconnect ── HLS IP     │
│                                                             │
│  Host PC (TCL脚本)                                          │
│    ↓                                                         │
│  verify_bram.tcl                                            │
│    ↓                                                         │
│  1. 加载数据到BRAM                                           │
│    2. 启动HLS IP                                             │
│    3. 读结果                                                  │
│    4. Host端calcKernels                                      │
└─────────────────────────────────────────────────────────────┘
```

### TCL验证流程

参考: `reference/tcl脚本设计参考/Example_Tcl_Command_Script.tcl`

```tcl
# 1. 连接硬件
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. 加载Source数据到BRAM
create_hw_axi_txn load_src [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data source_33x33.bin -len 1089 -type write

# 3. 加载Mask数据
create_hw_axi_txn load_mask [get_hw_axis hw_axi_1] \
  -address 0x40010000 -data mask_128x128.bin -len 16384 -type write

# 4. 配置参数（AXI-Lite寄存器）
create_hw_axi_txn config_na [get_hw_axis hw_axi_1] \
  -address 0x40020010 -data 0.8 -len 1 -type write

# 5. 启动IP（写ap_start寄存器）
create_hw_axi_txn start_ip [get_hw_axis hw_axi_1] \
  -address 0x40020000 -data 0x00000001 -len 1 -type write

# 6. 等待完成（轮询ap_done）
while {![check_done]} {
    sleep 100ms
}

# 7. 读空中像结果
create_hw_axi_txn read_image [get_hw_axis hw_axi_1] \
  -address 0x40030000 -len 16384 -type read

# 8. Host端calcKernels（Python/C++）
python calc_kernels_from_tcc.py tcc.bin --nk 10
```

### 优点

- ✅ 无需PCIe硬件（JTAG即可）
- ✅ 低成本验证
- ✅ 适合Phase 5原型测试

### 缺点

- ⚠️ 速度慢（JTAG带宽有限）
- ⚠️ 不适合大批量数据传输

---

## PCIe模式（后期升级，Phase 6+）

### 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│ Vivado Block Design                                         │
│                                                             │
│  PCIe DMA Engine ────┬─── AXI Interconnect ── HLS IP       │
│                      │                                      │
│                      └─── MIG (DDR Controller) ── DDR       │
│                                                             │
│  Host PC (XRT程序)                                          │
│    ↓                                                         │
│  host/xrt_test.cpp                                          │
│    ↓                                                         │
│  1. PCIe DMA传输数据                                         │
│    2. 启动HLS IP                                             │
│    3. PCIe DMA接收结果                                       │
│    4. Host端calcKernels                                      │
└─────────────────────────────────────────────────────────────┘
```

### XRT程序框架

```cpp
// host/xrt_test.cpp（Phase 6+实现）
#include "xrt/xrt_kernel.h"

int main() {
    // 1. 加载xclbin
    xrt::device device(0);
    xrt::kernel kernel(device, "tcc_top.xclbin", "tcc_top");
    
    // 2. 准备数据
    xrt::bo source_bo(device, 33*33*sizeof(float), kernel.group_id(0));
    xrt::bo mask_bo(device, 128*128*sizeof(float), kernel.group_id(1));
    xrt::bo tcc_bo(device, 16641*16641*sizeof(cmpxData_t), kernel.group_id(2));
    
    // 3. PCIe DMA传输数据
    source_bo.write(source_data);
    mask_bo.write(mask_data);
    source_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    mask_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    
    // 4. 配置参数
    kernel.set_arg(3, 0.8);    // NA
    kernel.set_arg(4, 193.0);  // lambda
    
    // 5. 启动IP
    auto run = kernel(source_bo, mask_bo, tcc_bo, ...);
    run.wait();  // 等待完成
    
    // 6. PCIe DMA接收结果
    tcc_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    cmpxData_t* tcc_result = tcc_bo.map();
    
    // 7. Host端calcKernels
    calcKernels(tcc_result, ...);
}
```

### 优点

- ✅ 高带宽（PCIe Gen3 x8: ~8GB/s）
- ✅ 大批量数据传输
- ✅ 生产级性能

### 缺点

- ⚠️ 需PCIe硬件（Alveo卡或自定义板）
- ⚠️ 需XRT环境配置

---

## 升级路径（HLS代码无需修改）

### HLS接口设计（已满足要求）

```cpp
// tcc_top.cpp（Phase 4实现）
void tcc_top(
    // AXI-Lite参数接口（更换PCIe只需改Vivado BD连线）
    volatile ap_uint<32>* ctrl_reg,
    
    // AXI4-Stream数据输入（更换PCIe DMA只需改Vivado BD）
    hls::stream<cmpxData_t>& s_axis_mask,
    hls::stream<cmpxData_t>& s_axis_src,
    
    // AXI4-Master存储（更换DDR只需改Vivado BD）
    ap_uint<32>* m_axi_mem,
    
    // AXI4-Stream输出
    hls::stream<cmpxData_t>& m_axis_image,
    hls::stream<cmpxData_t>& m_axis_tcc
) {
    // ⚠️ HLS代码不依赖数据来源
    // ✅ JTAG/PCIe切换只需修改Vivado Block Design
}
```

### Vivado Block Design更换（Phase 6）

```tcl
# JTAG模式（删除）
delete_bd_objs [get_bd_cells jtag_to_axi_master]
delete_bd_objs [get_bd_cells axi_bram_controller]

# PCIe模式（新增）
create_bd_cell -type ip -vlnv xilinx.com:ip:xdma axi_xdma
create_bd_cell -type ip -vlnv xilinx.com:ip:ddr4 axi_mig_ddr4

# 连接PCIe DMA到AXI-Stream
connect_bd_net [get_bd_pins axi_xdma/M_AXIS] [get_bd_pins tcc_top/s_axis_src]
connect_bd_net [get_bd_pins axi_xdma/M_AXIS] [get_bd_pins tcc_top/s_axis_mask]

# 连接AXI-Master到DDR
connect_bd_net [get_bd_pins tcc_top/m_axi_mem] [get_bd_pins axi_mig_ddr4/S_AXI]
```

---

## 文件组织

### 当前阶段（Phase 0-5）

```
source/TCC_HLS/host/
└── README_HOST.md          # ✅ 升级路径说明（本文件）
└── verify_bram.tcl         # ⬜ Phase 5添加（JTAG验证脚本）
```

### 后期阶段（Phase 6+）

```
source/TCC_HLS/host/
├── README_HOST.md          # 升级路径说明
├── jtag_adapter/           # JTAG模式（Phase 5）
│   ├── verify_bram.tcl     # TCL验证脚本
│   └── load_golden.py      # 数据加载脚本
├── pcie_adapter/           # PCIe模式（Phase 6+）
│   ├── xrt_test.cpp        # XRT测试程序
│   ├── xrt_kernel.h        # HLS kernel封装
│   ├── calc_kernels.cpp    # Host端SOCS核计算
│   ├── Makefile            # XRT编译脚本
│   └── run_pcie.sh         # PCIe测试脚本
└── common/                 # 通用代码
    ├── data_loader.cpp     # 数据加载工具
    ├── file_io.cpp         # 文件IO工具
    └── kernel_utils.cpp    # Kernel处理工具
```

---

## 性能对比

| 模式 | 数据传输带宽 | 适用场景 | 实现难度 | 推荐阶段 |
|------|--------------|----------|----------|----------|
| **JTAG** | ~10 MB/s | Phase 5原型测试 | 低（TCL脚本） | ✅ Phase 5 |
| **PCIe DMA** | ~8 GB/s | Phase 6+生产级 | 中（XRT程序） | ⬜ Phase 6+ |

---

## 关键约束（参考TODO.md）

1. **✅ HLS代码不依赖数据来源**: 全部通过AXI-Stream
2. **✅ 参数通过AXI-Lite传入**: 不硬编码
3. **✅ 接口更换只需改Vivado BD**: HLS无需修改
4. **⚠️ Host代码单独存放**: `host/`目录，便于后期替换

---

**创建日期**: 2026-04-06  
**状态**: 空目录（Phase 5+实现）